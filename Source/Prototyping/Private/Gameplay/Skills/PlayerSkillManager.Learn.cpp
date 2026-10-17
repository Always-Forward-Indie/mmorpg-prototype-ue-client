#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"

void UPlayerSkillManager::AddLearnedSkill(const FPlayerSkillNetworkData& NetworkData)
{
    if (NetworkData.skillSlug.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager::AddLearnedSkill: empty skillSlug, ignoring"));
        return;
    }

    if (PlayerSkills.Contains(NetworkData.skillSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager::AddLearnedSkill: skill '%s' already known, skipping"), *NetworkData.skillSlug);
        return;
    }

    FPlayerSkillData NewSkill = CreatePlayerSkillData(NetworkData);
    PlayerSkills.Add(NetworkData.skillSlug, NewSkill);

    // Register with legacy skill system so hotbar / combat work immediately
    if (SkillSystemManager)
    {
        FSkillData LegacySkillData;
        LegacySkillData.skillSlug = NetworkData.skillSlug;
        LegacySkillData.skillName = NewSkill.definitionData.displayName.ToString();
        LegacySkillData.animationName = NewSkill.definitionData.animationName;
        LegacySkillData.castTime = NetworkData.castMs / 1000.0f;
        LegacySkillData.cooldown = NetworkData.cooldownMs / 1000.0f;
        LegacySkillData.manaCost = NetworkData.costMp;
        LegacySkillData.effectType = NewSkill.definitionData.effectType;
        LegacySkillData.school = NewSkill.definitionData.school;
        LegacySkillData.range = NetworkData.maxRange;
        SkillSystemManager->RegisterSkill(NetworkData.skillSlug, LegacySkillData);
    }

    // Broadcast so AvailableSkillsWidget / hotbar refresh
    TArray<FPlayerSkillData> AllSkills;
    PlayerSkills.GenerateValueArray(AllSkills);
    OnSkillsInitialized.Broadcast(AllSkills);

    UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager::AddLearnedSkill: skill '%s' added successfully"), *NetworkData.skillSlug);
}

