#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/Combat/ISkillEffectHandler.h"
#include "CombatSystemManager.generated.h"

// Forward declarations
class UMyGameInstance;
class UNetworkManager;

// Delegates for combat events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillInitiated, const FSkillInitiationData&, SkillData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillCompleted, const FSkillResultData&, SkillResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActorDied, int32, DeadActorId);

/**
 * Main manager for the combat system following SOLID principles
 * Handles skill initiation, processing, and result application
 * Uses only UE5 UINTERFACE system for proper type safety
 *
 * DEFERRED HIT-POINT ARCHITECTURE:
 *   combatInitiation  -> PlaySkillAnimation (start montage, sounds, VFX)
 *   combatResult      -> Store as PendingSkillResult (DO NOT apply yet)
 *   AnimNotify HitPoint -> NotifyHitPoint() -> ApplySkillEffects (damage/heal/FCT)
 *
 *   This ensures damage numbers, HP bar changes and camera shake fire at the
 *   exact animation frame of impact, not when the server packet arrives.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UCombatSystemManager : public UObject
{
    GENERATED_BODY()

public:
    UCombatSystemManager();

    // Initialization
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void Initialize(UMyGameInstance* GameInstance, UNetworkManager* NetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void SetWorldContext(UWorld* World);

    // Actor registration using proper UE interface system
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void RegisterCombatable(const TScriptInterface<ICombatable>& CombatableActor);
    
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void UnregisterCombatable(const TScriptInterface<ICombatable>& CombatableActor);

    // Effect handler management
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void RegisterEffectHandler(const TScriptInterface<ISkillEffectHandler>& Handler);
    
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void UnregisterEffectHandler(const TScriptInterface<ISkillEffectHandler>& Handler);
    
    // Clean up invalid effect handlers
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void CleanupInvalidHandlers();

    // Combat event processing
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void ProcessSkillInitiation(const FSkillInitiationData& SkillData);

    static ECasterType MapNetCasterType(int32 Net, const FString& Str);

    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void ProcessSkillResult(const FSkillResultData& SkillResult);

    // Actor lookup using proper interface system
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    TScriptInterface<ICombatable> FindCombatableById(int32 ActorId, ECasterType ActorType);

    // Network requests
    UFUNCTION(BlueprintCallable, Category = "Combat System")
    void SendAttackRequest(int32 AttackerId, int32 TargetId, const FString& SkillSlug, ECasterType TargetType);

    // Event delegates
    UPROPERTY(BlueprintAssignable, Category = "Combat System|Events")
    FOnSkillInitiated OnSkillInitiated;

    UPROPERTY(BlueprintAssignable, Category = "Combat System|Events")
    FOnSkillCompleted OnSkillCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Combat System|Events")
    FOnActorDied OnActorDied;

    /** Called by the hit-point anim notify delegate when the animation reaches the impact frame.
     *  Does NOT flush results that are waiting on a projectile impact. */
    void NotifyHitPoint(int32 CasterId);

    /** Called exclusively by ABaseMMOProjectile on impact.
     *  Flushes all pending results for the caster, including projectile-waiting ones. */
    void NotifyProjectileImpact(int32 CasterId);

protected:
    // Find the appropriate effect handler for the skill type
    TScriptInterface<ISkillEffectHandler> FindEffectHandler(ESkillEffectType EffectType);

    // Apply skill effects to target
    void ApplySkillEffects(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target);

    // Logging helper
    void LogCombatEvent(const FString& Event, const FString& Details);

private:
    UPROPERTY()
    TObjectPtr<UWorld> WorldContext;

    UPROPERTY()
    TObjectPtr<UMyGameInstance> GameInstance;

    UPROPERTY()
    TObjectPtr<UNetworkManager> NetworkManager;

    // Registry of all combatable actors.
    // TWeakObjectPtr is used for the validity check so that a GC'd actor
    // (whose FObjectHandle may read as 0xFFFFFFFFFFFFFFFF) is safely detected
    // before we ever call through the interface pointer.
    struct FCombatableEntry
    {
        TWeakObjectPtr<UObject> WeakObject;
        ICombatable*            Interface = nullptr;
    };
    TMap<FString, FCombatableEntry> RegisteredCombatables;

    // Registry of effect handlers.
    struct FEffectHandlerEntry
    {
        TWeakObjectPtr<UObject> WeakObject;
        ISkillEffectHandler*    Interface = nullptr;
    };
    TArray<FEffectHandlerEntry> EffectHandlers;

    // Strong references to effect handler UObjects so they are not garbage-collected.
    // The EffectHandlers array above only holds TWeakObjectPtr which does NOT prevent GC.
    UPROPERTY()
    TArray<TObjectPtr<UObject>> EffectHandlerStrongRefs;

    // --- Deferred Hit-Point Result Queue ---
    // When combatResult arrives, we store it here keyed by casterId.
    // When NotifyHitPoint(casterId) fires from the animation, we pop
    // the result and apply damage/heal/effects at the visual impact moment.
    struct FPendingResult
    {
        FSkillResultData ResultData;
        double           StoredAtWorldTime = 0.0;
        // True when the skill spawns a projectile.  These results must only be
        // flushed by the projectile on impact — animation hit-point notifies must
        // not touch them, otherwise a melee auto-attack fired while the projectile
        // is still in flight would prematurely apply the projectile's damage.
        bool             bWaitsForProjectile = false;
    };
    TMap<int32, TArray<FPendingResult>> PendingSkillResults;

    // Safety timeout (seconds): if the hit-point notify never fires (no montage,
    // broken anim, or projectile that never hits), we apply the result after this
    // delay. Must be > projectile MaxLifetime (8.0s) so projectiles always hit first.
    static constexpr double PendingResultTimeoutSeconds = 12.0;

    // When a projectile lands but combatResult has not arrived yet (server slower than
    // projectile flight), we record the WorldTime of impact here.  ProcessSkillResult
    // checks this map on arrival: if the entry is fresh it applies the result
    // immediately instead of waiting for another physical impact that will never come.
    TMap<int32, double> EarlyProjectileImpacts; // CasterId -> WorldTime of impact
    static constexpr double EarlyImpactWindowSeconds = 3.0; // > 2x max expected RTT

    // Flush pending results for a caster.
    // bSkipProjectileWaiters = true  -> skip results that need a projectile hit
    //                                   (called from animation hit-point notifies).
    // bSkipProjectileWaiters = false -> flush everything, including projectile results
    //                                   (called from projectile impact or safety timeout).
    void FlushPendingResults(int32 CasterId, bool bSkipProjectileWaiters = false);

    // Start a safety-timeout timer for a pending result
    void StartPendingResultTimeout(int32 CasterId);

private:
    // Helper function to create a unique key for combatable registration
    FString CreateCombatableKey(int32 ActorId, ECasterType ActorType) const;
};