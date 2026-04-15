#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Engine.h"
#include "Data/DataStructs.h"
#include "Gameplay/Skills/IPlayerSkillSystem.h"
#include "PlayerSkillManager.generated.h"

// Forward declarations
class USkillSystemManager;
class UTimeSyncService;
class USkillDefinitionRepository;

// === Delegates (������������ ����������� ��� ����������) ===
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillsInitialized, const TArray<FPlayerSkillData>&, Skills);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillCooldownStarted, const FString&, SkillSlug);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillReady, const FString&, SkillSlug);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillSlotChanged, int32, SlotIndex, const FSkillSlotData&, SlotData);

/**
 * Main player skill management system
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UPlayerSkillManager : public UObject, public IPlayerSkillSystem
{
    GENERATED_BODY()

public:
    UPlayerSkillManager();

    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager")
    void Initialize(USkillSystemManager* InSkillSystemManager,
        USkillDefinitionRepository* InDefinitionRepository,
        UTimeSyncService* InTimeSyncService = nullptr);

    // IPlayerSkillSystem
    virtual void InitializePlayerSkills(const FPlayerSkillsInitializationData& SkillsData) override;
    virtual bool HasSkill(const FString& SkillSlug) const override;
    virtual FPlayerSkillData GetSkillData(const FString& SkillSlug) const override;
    virtual TArray<FPlayerSkillData> GetAllPlayerSkills() const override;
    virtual bool CanCastSkill(const FString& SkillSlug) const override;
    virtual bool TryCastSkill(const FString& SkillSlug, int32 TargetId, ECasterType TargetType) override;
    virtual bool IsSkillOnCooldown(const FString& SkillSlug) const override;
    virtual float GetSkillCooldownRemaining(const FString& SkillSlug) const override;
    virtual void StartSkillCooldown(const FString& SkillSlug) override;
    virtual void SetSkillSlot(int32 SlotIndex, const FString& SkillSlug, const FKey& BoundKey) override;
    virtual FSkillSlotData GetSkillSlot(int32 SlotIndex) const override;
    virtual TArray<FSkillSlotData> GetAllSkillSlots() const override;

    // Blueprint helpers
    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager")
    void CastSkillBySlot(int32 SlotIndex, int32 TargetId, ECasterType TargetType);

    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager")
    bool IsSlotAssigned(int32 SlotIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager")
    FString GetSkillSlugFromSlot(int32 SlotIndex) const;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Player Skill Manager")
    FOnSkillsInitialized OnSkillsInitialized;

    UPROPERTY(BlueprintAssignable, Category = "Player Skill Manager")
    FOnSkillCooldownStarted OnSkillCooldownStarted;

    UPROPERTY(BlueprintAssignable, Category = "Player Skill Manager")
    FOnSkillReady OnSkillReady;

    UPROPERTY(BlueprintAssignable, Category = "Player Skill Manager")
    FOnSkillSlotChanged OnSkillSlotChanged;

    // Utils
    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager")
    void UpdateCooldowns(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager")
    int32 GetCharacterId() const { return CharacterId; }

    // Public access for time synchronization with UI
    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager")
    float GetCurrentTime() const;

    // Handle combat events from server (with per-cast cooldown/gcd from initiation packet)
    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager") 
    void HandleSkillInitiation(const FString& SkillSlug, int32 CasterId,
        int32 CooldownMs = 0, int32 GcdMs = 0);

    // Called by SkillShopNetworkHandler when a skill is successfully learned at runtime.
    // Adds the skill to the manager so it immediately becomes available for the hotbar.
    UFUNCTION(BlueprintCallable, Category = "Player Skill Manager")
    void AddLearnedSkill(const FPlayerSkillNetworkData& NetworkData);

    // Called when the attack animation finishes to release the cast lock
    void NotifyAnimationEnded();

    double GetServerSeconds() const;

    double GetWorldSeconds() const;

    double NowFor(const FPlayerSkillData* Skill) const;

    // GCD query
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Skill Manager")
    bool IsGCDActive() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Skill Manager")
    float GetGCDRemaining() const;

    // Returns true while a non-auto-attack skill animation is playing.
    // Use to block player rotation input during the animation.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Skill Manager")
    bool IsSkillAnimationPlaying() const;

protected:
    // Get world for time calculations
    UWorld* GetWorld() const;

    // Dependencies
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependencies")
    TObjectPtr<USkillSystemManager> SkillSystemManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependencies")
    TObjectPtr<USkillDefinitionRepository> DefinitionRepository;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependencies")
    TObjectPtr<UTimeSyncService> TimeSyncService;

    // Player skills
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Skills")
    TMap<FString, FPlayerSkillData> PlayerSkills;

    // Skill slots
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Skills")
    TMap<int32, FSkillSlotData> SkillSlots;

    // Character data
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Data")
    int32 CharacterId = 0;

    // Config
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxSkillSlots = 10;

private:
    // Internals
    FPlayerSkillData CreatePlayerSkillData(const FPlayerSkillNetworkData& NetworkData) const;
    void UpdateSkillCooldownState(FPlayerSkillData& SkillData);
    bool ValidateSkillSlot(int32 SlotIndex) const;

    bool bIsInitialized = false;

    // Global Cooldown (GCD) tracking
    double GCDEndTime = 0.0;
    bool   bGCDUsesServerClock = true;

    // ── Animation cast lock ──────────────────────────────────────────────────
    // Set to true by HandleSkillInitiation when the server confirms a non-auto-attack
    // skill for the local player. Cleared by NotifyAnimationEnded when the montage
    // fully completes. Prevents a skill from being re-cast while its animation is
    // still playing (fixes the "damage appears at wrong time" bug when the previous
    // animation's HitPoint notify fires during the new animation).
    bool   bIsAnimationPlaying       = false;
    double AnimationStartWorldTime   = 0.0;
    // Safety: if NotifyAnimationEnded never fires (no montage end notify), the lock
    // auto-expires after this many seconds so the player is never permanently blocked.
    static constexpr double AnimationLockTimeoutSec = 6.0;

    // ── Server-confirmation guard ────────────────────────────────────────────
    // Set to true by TryCastSkill right before the request is sent.
    // Cleared by HandleSkillInitiation when the server-confirmed combatInitiation
    // arrives. Prevents a second skill request from being sent during the network
    // round-trip (~100 ms) before the first confirmation (and its cooldown) arrives.
    bool   bAwaitingServerConfirmation     = false;
    double ConfirmationRequestWorldTime    = 0.0;
    // Auto-expire if the server never responds (packet loss / rejection with no
    // combatInitiation reply).  2 s is generous enough for high-latency connections.
    static constexpr double ConfirmationTimeoutSec = 2.0;
};
