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

    // Registry of all combatable actors using proper UE interface system
    UPROPERTY()
    TMap<FString, TScriptInterface<ICombatable>> RegisteredCombatables;

    // Registry of effect handlers using proper UE interface system
    UPROPERTY()
    TArray<TScriptInterface<ISkillEffectHandler>> EffectHandlers;

private:
    // Helper function to create a unique key for combatable registration
    FString CreateCombatableKey(int32 ActorId, ECasterType ActorType) const;
};