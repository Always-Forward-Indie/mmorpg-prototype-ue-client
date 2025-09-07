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

bool UPlayerSkillManager::HasSkill(const FString& SkillSlug) const
{
    return PlayerSkills.Contains(SkillSlug);
}

FPlayerSkillData UPlayerSkillManager::GetSkillData(const FString& SkillSlug) const
{
    if (const FPlayerSkillData* Found = PlayerSkills.Find(SkillSlug))
    {
        return *Found;
    }
    return FPlayerSkillData();
}

TArray<FPlayerSkillData> UPlayerSkillManager::GetAllPlayerSkills() const
{
    TArray<FPlayerSkillData> Skills;
    PlayerSkills.GenerateValueArray(Skills);
    return Skills;
}

bool UPlayerSkillManager::CanCastSkill(const FString& SkillSlug) const
{
    if (!HasSkill(SkillSlug))
    {
        return false;
    }

    const FPlayerSkillData& SkillData = GetSkillData(SkillSlug);
    
    // Check if skill is on cooldown
    if (IsSkillOnCooldown(SkillSlug))
    {
        return false;
    }

    // Use existing skill system validation
    if (SkillSystemManager)
    {
        return SkillSystemManager->CanCastSkill(CharacterId, SkillSlug);
    }

    return true;
}

bool UPlayerSkillManager::TryCastSkill(const FString& SkillSlug, int32 TargetId, ECasterType TargetType)
{
    if (!CanCastSkill(SkillSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Cannot cast skill %s"), *SkillSlug);
        return false;
    }

    // Cast skill through existing system
    if (SkillSystemManager)
    {
        bool bSuccess = SkillSystemManager->CastSkill(CharacterId, TargetId, SkillSlug, TargetType);
        
        if (bSuccess)
        {
            // Start cooldown
            StartSkillCooldown(SkillSlug);
            UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Successfully cast skill %s"), *SkillSlug);
        }
        
        return bSuccess;
    }

    return false;
}

bool UPlayerSkillManager::IsSkillOnCooldown(const FString& SkillSlug) const
{
    if (const FPlayerSkillData* SkillData = PlayerSkills.Find(SkillSlug))
    {
        float CurrentTime = GetCurrentTime();
        return CurrentTime < SkillData->cooldownEndTime;
    }
    return false;
}

float UPlayerSkillManager::GetSkillCooldownRemaining(const FString& SkillSlug) const
{
    if (const FPlayerSkillData* SkillData = PlayerSkills.Find(SkillSlug))
    {
        float CurrentTime = GetCurrentTime();
        float Remaining = SkillData->cooldownEndTime - CurrentTime;
        return FMath::Max(0.0f, Remaining);
    }
    return 0.0f;
}

void UPlayerSkillManager::StartSkillCooldown(const FString& SkillSlug)
{
    if (FPlayerSkillData* SkillData = PlayerSkills.Find(SkillSlug))
    {
        float CurrentTime = GetCurrentTime();
        float CooldownDuration = SkillData->networkData.cooldownMs / 1000.0f; // Convert ms to seconds
        
        SkillData->cooldownEndTime = CurrentTime + CooldownDuration;
        SkillData->bIsOnCooldown = true;
        SkillData->bIsReady = false;

        OnSkillCooldownStarted.Broadcast(SkillSlug);

        UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Started cooldown for skill %s (%.1fs)"), 
            *SkillSlug, CooldownDuration);
    }
}

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

void UPlayerSkillManager::UpdateCooldowns(float DeltaTime)
{
    float CurrentTime = GetCurrentTime();
    
    for (auto& SkillPair : PlayerSkills)
    {
        FPlayerSkillData& SkillData = SkillPair.Value;
        
        if (SkillData.bIsOnCooldown && CurrentTime >= SkillData.cooldownEndTime)
        {
            SkillData.bIsOnCooldown = false;
            SkillData.bIsReady = true;
            SkillData.cooldownEndTime = 0.0f;

            OnSkillReady.Broadcast(SkillPair.Key);
            
            UE_LOG(LogTemp, Verbose, TEXT("PlayerSkillManager: Skill %s is ready"), 
                *SkillPair.Key);
        }
    }
}

FPlayerSkillData UPlayerSkillManager::CreatePlayerSkillData(const FPlayerSkillNetworkData& NetworkData) const
{
    FPlayerSkillData SkillData;
    SkillData.networkData = NetworkData;
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Creating skill data for %s"), *NetworkData.skillSlug);
    
    // Get definition from repository
    if (DefinitionRepository)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: DefinitionRepository available, getting definition"));
        SkillData.definitionData = DefinitionRepository->GetDefinition(NetworkData.skillSlug);
        
        // Debug the loaded definition
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Loaded definition - DisplayName: %s, Icon IsValid: %s"), 
            *SkillData.definitionData.displayName.ToString(),
            SkillData.definitionData.skillIcon.IsValid() ? TEXT("true") : TEXT("false"));
            
        if (SkillData.definitionData.skillIcon.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Icon path: %s"), 
                *SkillData.definitionData.skillIcon.ToSoftObjectPath().ToString());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillManager: DefinitionRepository is NULL"));
        
        // Create basic definition if repository is not available
        SkillData.definitionData.skillSlug = NetworkData.skillSlug;
        SkillData.definitionData.displayName = FText::FromString(NetworkData.skillSlug);
    }

    return SkillData;
}

float UPlayerSkillManager::GetCurrentTime() const
{
    // Use synchronized time if available, otherwise fallback to world time
    if (TimeSyncService && TimeSyncService->IsTimeSyncValid())
    {
        int64 ServerTimeMs = TimeSyncService->GetEstimatedServerTimeMs();
        return static_cast<float>(ServerTimeMs) / 1000.0f;
    }
    
    // Fallback to world time
    if (SkillSystemManager)
    {
        return SkillSystemManager->GetWorldTime();
    }
    
    return 0.0f;
}

void UPlayerSkillManager::UpdateSkillCooldownState(FPlayerSkillData& SkillData)
{
    float CurrentTime = GetCurrentTime();
    SkillData.bIsOnCooldown = CurrentTime < SkillData.cooldownEndTime;
    SkillData.bIsReady = !SkillData.bIsOnCooldown;
}

bool UPlayerSkillManager::ValidateSkillSlot(int32 SlotIndex) const
{
    return SlotIndex >= 0 && SlotIndex < MaxSkillSlots;
}