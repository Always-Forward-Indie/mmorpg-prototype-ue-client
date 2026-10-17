#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"

void UPlayerSkillManager::SetSkillSlot(int32 SlotIndex, const FString& SkillSlug, const FKey& BoundKey)
{
    if (!ValidateSkillSlot(SlotIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Invalid slot index %d"), SlotIndex);
        return;
    }

    if (!SkillSlug.IsEmpty() && !HasSkill(SkillSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Cannot assign unknown skill %s to slot %d"), 
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

    UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Set skill slot %d to %s"), 
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

    // Auto-derive from effectType
    switch (SkillData.definitionData.effectType)
    {
    case ESkillEffectType::Damage:
    case ESkillEffectType::Debuff:
        return ESkillTargetPolicy::TargetOnly;

    case ESkillEffectType::Healing:
    case ESkillEffectType::Buff:
    case ESkillEffectType::Resource:
        return ESkillTargetPolicy::SelfAndTarget;

    case ESkillEffectType::Teleport:
    case ESkillEffectType::None:
        return ESkillTargetPolicy::SelfOnly;

    default:
        return ESkillTargetPolicy::SelfOnly;
    }
}

bool UPlayerSkillManager::IsSkillSelfCastable(const FString& SkillSlug) const
{
    const ESkillTargetPolicy Policy = GetEffectiveTargetPolicy(SkillSlug);
    return Policy == ESkillTargetPolicy::SelfOnly || Policy == ESkillTargetPolicy::SelfAndTarget;
}

