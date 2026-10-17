#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"

UPlayerSkillManager::UPlayerSkillManager()
{
    SkillSystemManager = nullptr;
    DefinitionRepository = nullptr;
    TimeSyncService = nullptr;
    CharacterId = 0;
    MaxSkillSlots = 10;
    bIsInitialized = false;
}

void UPlayerSkillManager::Initialize(USkillSystemManager* InSkillSystemManager, 
                                   USkillDefinitionRepository* InDefinitionRepository,
                                   UTimeSyncService* InTimeSyncService)
{
    if (!InSkillSystemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillManager: Cannot initialize with null SkillSystemManager"));
        return;
    }

    if (!InDefinitionRepository)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillManager: Cannot initialize with null DefinitionRepository"));
        return;
    }

    SkillSystemManager = InSkillSystemManager;
    DefinitionRepository = InDefinitionRepository;
    TimeSyncService = InTimeSyncService;

    // Initialize skill slots
    SkillSlots.Empty();
    for (int32 i = 0; i < MaxSkillSlots; ++i)
    {
        FSkillSlotData SlotData;
        SlotData.slotIndex = i;
        SkillSlots.Add(i, SlotData);
    }

    bIsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Initialized successfully"));
}

void UPlayerSkillManager::InitializePlayerSkills(const FPlayerSkillsInitializationData& SkillsData)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillManager: Cannot initialize skills - manager not initialized"));
        return;
    }

    CharacterId = SkillsData.characterId;
    PlayerSkills.Empty();

    // Process each skill from server
    for (const FPlayerSkillNetworkData& NetworkSkill : SkillsData.skills)
    {
        FPlayerSkillData PlayerSkill = CreatePlayerSkillData(NetworkSkill);
        PlayerSkills.Add(NetworkSkill.skillSlug, PlayerSkill);

        // Register skill with existing skill system
        FSkillData LegacySkillData;
        LegacySkillData.skillSlug = NetworkSkill.skillSlug;
        LegacySkillData.skillName = PlayerSkill.definitionData.displayName.ToString();
        LegacySkillData.animationName = PlayerSkill.definitionData.animationName;
        LegacySkillData.castTime = NetworkSkill.castMs / 1000.0f; // Convert ms to seconds
        LegacySkillData.cooldown = NetworkSkill.cooldownMs / 1000.0f; // Convert ms to seconds
        LegacySkillData.manaCost = NetworkSkill.costMp;
        LegacySkillData.effectType = PlayerSkill.definitionData.effectType;
        LegacySkillData.school = PlayerSkill.definitionData.school;
        LegacySkillData.range = NetworkSkill.maxRange;

        SkillSystemManager->RegisterSkill(NetworkSkill.skillSlug, LegacySkillData);

        UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Initialized skill %s (Level %d)"), 
            *NetworkSkill.skillSlug, NetworkSkill.skillLevel);
    }

    // Broadcast initialization event
    TArray<FPlayerSkillData> AllSkills = GetAllPlayerSkills();
    OnSkillsInitialized.Broadcast(AllSkills);

    UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Initialized %d skills for character %d"), 
        PlayerSkills.Num(), CharacterId);
}

