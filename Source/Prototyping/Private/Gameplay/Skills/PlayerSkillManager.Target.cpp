#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"
#include "Utils/NetCore/SkillTargetRule.h"

bool UPlayerSkillManager::ResolveSkillTarget(const FString& SkillSlug, int32& InOutTargetId, ECasterType& InOutTargetType) const
{
    const ESkillTargetPolicy Policy = GetEffectiveTargetPolicy(SkillSlug);
    // Decision table lives in FSkillTargetRule (pinned by MMO.Skills.TargetResolve);
    // this wrapper keeps the log lines and writes the resolved values back.
    const FSkillTargetResolution R = FSkillTargetRule::Resolve(Policy, InOutTargetId, InOutTargetType, CharacterId);
    switch (R.Outcome)
    {
    case ETargetResolveOutcome::MissingTarget:
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Skill %s requires a target (TargetOnly policy)"), *SkillSlug);
        return false;
    case ETargetResolveOutcome::SelfTargetRejected:
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Skill %s cannot target self (TargetOnly policy)"), *SkillSlug);
        return false;
    case ETargetResolveOutcome::UnknownPolicy:
        return false;
    case ETargetResolveOutcome::OkFallback:
        InOutTargetId = R.TargetId;
        InOutTargetType = R.TargetType;
        UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Skill %s has no target, defaulting to self"), *SkillSlug);
        return true;
    default:
        InOutTargetId = R.TargetId;
        InOutTargetType = R.TargetType;
        return true;
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

