#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"

bool UPlayerSkillManager::ResolveSkillTarget(const FString& SkillSlug, int32& InOutTargetId, ECasterType& InOutTargetType) const
{
    const ESkillTargetPolicy Policy = GetEffectiveTargetPolicy(SkillSlug);
    const bool bHasExternalTarget = (InOutTargetId > 0 && InOutTargetId != CharacterId
        && InOutTargetType != ECasterType::None && InOutTargetType != ECasterType::Self);

    switch (Policy)
    {
    case ESkillTargetPolicy::SelfOnly:
        // Always target self regardless of what was passed
        InOutTargetId = CharacterId;
        InOutTargetType = ECasterType::Self;
        return true;

    case ESkillTargetPolicy::TargetOnly:
        // Must have a valid external target; cannot target self
        if (!bHasExternalTarget)
        {
            UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Skill %s requires a target (TargetOnly policy)"), *SkillSlug);
            return false;
        }
        if (InOutTargetId == CharacterId)
        {
            UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Skill %s cannot target self (TargetOnly policy)"), *SkillSlug);
            return false;
        }
        return true;

    case ESkillTargetPolicy::SelfAndTarget:
        // If external target is available, use it; otherwise default to self
        if (bHasExternalTarget)
        {
            return true;
        }
        // No external target — cast on self
        InOutTargetId = CharacterId;
        InOutTargetType = ECasterType::Self;
        UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Skill %s has no target, defaulting to self"), *SkillSlug);
        return true;

    default:
        return false;
    }
}

void UPlayerSkillManager::CastSkillBySlot(int32 SlotIndex, int32 TargetId, ECasterType TargetType)
{
    if (!ValidateSkillSlot(SlotIndex))
    {
        return;
    }

    const FSkillSlotData& SlotData = GetSkillSlot(SlotIndex);
    if (!SlotData.bIsAssigned)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: No skill assigned to slot %d"), SlotIndex);
        return;
    }

    TryCastSkill(SlotData.skillSlug, TargetId, TargetType);
}

bool UPlayerSkillManager::IsSlotAssigned(int32 SlotIndex) const
{
    const FSkillSlotData& SlotData = GetSkillSlot(SlotIndex);
    return SlotData.bIsAssigned;
}

FString UPlayerSkillManager::GetSkillSlugFromSlot(int32 SlotIndex) const
{
    const FSkillSlotData& SlotData = GetSkillSlot(SlotIndex);
    return SlotData.skillSlug;
}

