#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/DataStructs.h"
#include "IPlayerSkillSystem.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPlayerSkillSystem : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for player skill system management
 * Follows Single Responsibility Principle - handles only skill-related operations
 */
class PROTOTYPING_API IPlayerSkillSystem
{
    GENERATED_BODY()

public:
    // Skill Management
    virtual void InitializePlayerSkills(const FPlayerSkillsInitializationData& SkillsData) = 0;
    virtual bool HasSkill(const FString& SkillSlug) const = 0;
    virtual FPlayerSkillData GetSkillData(const FString& SkillSlug) const = 0;
    virtual TArray<FPlayerSkillData> GetAllPlayerSkills() const = 0;

    // Skill Casting
    virtual bool CanCastSkill(const FString& SkillSlug) const = 0;
    virtual bool TryCastSkill(const FString& SkillSlug, int32 TargetId, ECasterType TargetType) = 0;

    // Cooldown Management
    virtual bool IsSkillOnCooldown(const FString& SkillSlug) const = 0;
    virtual float GetSkillCooldownRemaining(const FString& SkillSlug) const = 0;
    virtual void StartSkillCooldown(const FString& SkillSlug) = 0;

    // Skill Slots (for UI)
    virtual void SetSkillSlot(int32 SlotIndex, const FString& SkillSlug, const FKey& BoundKey) = 0;
    virtual FSkillSlotData GetSkillSlot(int32 SlotIndex) const = 0;
    virtual TArray<FSkillSlotData> GetAllSkillSlots() const = 0;
};