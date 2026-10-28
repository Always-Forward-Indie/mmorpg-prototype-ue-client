#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Prototyping.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"
#include "Utils/NetCore/SkillTargetRule.h"

void UPlayerSkillManager::SetSkillSlot(int32 SlotIndex, const FString& SkillSlug, const FKey& BoundKey)
{
    if (!ValidateSkillSlot(SlotIndex))
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Invalid slot index %d"), SlotIndex);
        return;
    }

    if (!SkillSlug.IsEmpty() && !HasSkill(SkillSlug))
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Cannot assign unknown skill %s to slot %d"), 
            *SkillSlug, SlotIndex);
        return;
    }

    // Deduplicate: clear any other slot that already has this skill slug,
    // so the same skill never appears in two bar slots at once.
    if (!SkillSlug.IsEmpty())
    {
        for (auto& Pair : SkillSlots)
        {
            if (Pair.Key != SlotIndex && Pair.Value.skillSlug == SkillSlug)
            {
                Pair.Value.skillSlug.Empty();
                Pair.Value.bIsAssigned = false;
                OnSkillSlotChanged.Broadcast(Pair.Key, Pair.Value);
            }
        }
    }

    FSkillSlotData& SlotData = SkillSlots[SlotIndex];
    SlotData.skillSlug = SkillSlug;
    SlotData.boundKey = BoundKey;
    SlotData.bIsAssigned = !SkillSlug.IsEmpty();

    OnSkillSlotChanged.Broadcast(SlotIndex, SlotData);

    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillManager: Set skill slot %d to %s"), 
        SlotIndex, *SkillSlug);
}

void UPlayerSkillManager::QueuePendingSkillSlot(int32 SlotIndex, const FString& SkillSlug)
{
    if (!ValidateSkillSlot(SlotIndex) || SkillSlug.IsEmpty())
    {
        return;
    }

    // Newest wins: replace any earlier queued assignment for this slot.
    for (TPair<int32, FString>& Pending : PendingSkillSlots)
    {
        if (Pending.Key == SlotIndex)
        {
            Pending.Value = SkillSlug;
            return;
        }
    }
    PendingSkillSlots.Emplace(SlotIndex, SkillSlug);

    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillManager: Queued slot %d -> %s until skills initialize"),
        SlotIndex, *SkillSlug);
}

FSkillSlotData UPlayerSkillManager::GetSkillSlot(int32 SlotIndex) const
{
    if (const FSkillSlotData* Found = SkillSlots.Find(SlotIndex))
    {
        return *Found;
    }
    
    return FSkillSlotData(); // Return empty slot data
}

TArray<FSkillSlotData> UPlayerSkillManager::GetAllSkillSlots() const
{
    TArray<FSkillSlotData> Slots;
    SkillSlots.GenerateValueArray(Slots);
    return Slots;
}

ESkillTargetPolicy UPlayerSkillManager::GetEffectiveTargetPolicy(const FString& SkillSlug) const
{
    if (!HasSkill(SkillSlug))
    {
        return ESkillTargetPolicy::TargetOnly;
    }

    const FPlayerSkillData& SkillData = PlayerSkills[SkillSlug];
    const ESkillTargetPolicy Explicit = SkillData.definitionData.targetPolicy;

    // If designer set an explicit policy in the DataTable, respect it
    if (Explicit != ESkillTargetPolicy::Auto)
    {
        return Explicit;
    }

    // Auto-derive from effectType (derivation table lives in
    // FSkillTargetRule, pinned by MMO.Skills.TargetPolicy).
    return FSkillTargetRule::PolicyForEffect(SkillData.definitionData.effectType);
}

bool UPlayerSkillManager::IsSkillSelfCastable(const FString& SkillSlug) const
{
    const ESkillTargetPolicy Policy = GetEffectiveTargetPolicy(SkillSlug);
    // Predicate lives in FSkillTargetRule (pinned by MMO.Skills.TargetPolicy).
    return FSkillTargetRule::IsSelfCastable(Policy);
}

