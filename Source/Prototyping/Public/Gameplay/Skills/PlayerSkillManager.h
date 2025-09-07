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

// === Delegates (динамические мультикасты для блюпринтов) ===
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

protected:
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
    float GetCurrentTime() const;
    void UpdateSkillCooldownState(FPlayerSkillData& SkillData);
    bool ValidateSkillSlot(int32 SlotIndex) const;

    bool bIsInitialized = false;
};
