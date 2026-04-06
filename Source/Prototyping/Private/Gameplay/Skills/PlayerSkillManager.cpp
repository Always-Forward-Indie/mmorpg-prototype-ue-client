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

    // Check GCD
    if (IsGCDActive())
    {
        UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Cannot cast %s - GCD active (%.1fs remaining)"),
            *SkillSlug, GetGCDRemaining());
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

    // Cast skill through existing system - DON'T start cooldown here anymore
    if (SkillSystemManager)
    {
        bool bSuccess = SkillSystemManager->CastSkill(CharacterId, TargetId, SkillSlug, TargetType);
        
        if (bSuccess)
        {
            UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Successfully sent skill request %s - waiting for server confirmation"), *SkillSlug);
        }
        
        return bSuccess;
    }

    return false;
}

bool UPlayerSkillManager::IsSkillOnCooldown(const FString& SkillSlug) const
{
    if (const FPlayerSkillData* S = PlayerSkills.Find(SkillSlug)) {
        const double now = NowFor(S);
        return now < S->cooldownEndTime;
    }
    return false;
}

float UPlayerSkillManager::GetSkillCooldownRemaining(const FString& SkillSlug) const
{
    if (const FPlayerSkillData* S = PlayerSkills.Find(SkillSlug)) {
        const double now = NowFor(S);
        const double rem = S->cooldownEndTime - now;
        return rem > 0.0 ? static_cast<float>(rem) : 0.0f;
    }
    return 0.0f;
}

void UPlayerSkillManager::StartSkillCooldown(const FString& SkillSlug)
{
    if (FPlayerSkillData* S = PlayerSkills.Find(SkillSlug)) {
        const double dur = static_cast<double>(S->networkData.cooldownMs) / 1000.0;
        if (dur <= 0.0) { UE_LOG(LogTemp, Error, TEXT("Invalid cooldown")); return; }

        S->bCooldownUsesServerClock = true;              // ������ ��������
        const double now = GetServerSeconds();
        S->cooldownEndTime = now + dur;                  // double + server seconds
        S->bIsOnCooldown = true;
        S->bIsReady = false;

        OnSkillCooldownStarted.Broadcast(SkillSlug);
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
    for (auto& Pair : PlayerSkills) {
        FPlayerSkillData& S = Pair.Value;
        if (!S.bIsOnCooldown) continue;
        const double now = NowFor(&S);
        if (now >= S.cooldownEndTime) {
            S.bIsOnCooldown = false;
            S.bIsReady = true;
            S.cooldownEndTime = 0.0;
            OnSkillReady.Broadcast(Pair.Key);
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
    // Use the same time system as SkillSystemManager for consistency
    if (SkillSystemManager)
    {
        return SkillSystemManager->GetSynchronizedWorldTime();
    }
    
    // Fallback to synchronized time if SkillSystemManager is not available
    if (TimeSyncService && TimeSyncService->IsTimeSyncValid())
    {
        int64 ServerTimeMs = TimeSyncService->GetEstimatedServerTimeMs();
        return static_cast<float>(ServerTimeMs) / 1000.0f;
    }
    
    // Last resort fallback to world time
    if (SkillSystemManager)
    {
        return SkillSystemManager->GetWorldTime();
    }
    
    return 0.0f;
}

UWorld* UPlayerSkillManager::GetWorld() const
{
    // Try to get world through SkillSystemManager
    if (SkillSystemManager)
    {
        // We can't directly access private members, so let's try through outer
        if (UObject* Outer = GetOuter())
        {
            return Outer->GetWorld();
        }
    }
    
    return nullptr;
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

void UPlayerSkillManager::HandleSkillInitiation(const FString& SkillSlug, int32 CasterId,
    int32 CooldownMs, int32 GcdMs)
{
    // Only handle initiations for our character
    if (CasterId != CharacterId)
    {
        UE_LOG(LogTemp, Verbose, TEXT("PlayerSkillManager: Ignoring skill initiation for other character %d (ours: %d)"), 
            CasterId, CharacterId);
        return;
    }
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Server-confirmed skill %s (cooldownMs=%d, gcdMs=%d)"),
        *SkillSlug, CooldownMs, GcdMs);
    
    // Start per-skill cooldown using per-cast cooldownMs from the initiation packet
    if (HasSkill(SkillSlug))
    {
        if (FPlayerSkillData* S = PlayerSkills.Find(SkillSlug))
        {
            // Use per-cast cooldownMs if provided, otherwise fall back to networkData.cooldownMs
            const int32 EffectiveCooldownMs = (CooldownMs > 0) ? CooldownMs : S->networkData.cooldownMs;
            const double CooldownSec = static_cast<double>(EffectiveCooldownMs) / 1000.0;

            if (CooldownSec > 0.0)
            {
                S->bCooldownUsesServerClock = true;
                const double Now = GetServerSeconds();
                S->cooldownEndTime = Now + CooldownSec;
                S->bIsOnCooldown = true;
                S->bIsReady = false;

                OnSkillCooldownStarted.Broadcast(SkillSlug);
                UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Started cooldown %.1fs for %s"), CooldownSec, *SkillSlug);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Server confirmed skill %s but we don't have it"), *SkillSlug);
    }

    // Start GCD (affects all skills)
    if (GcdMs > 0)
    {
        const double GcdSec = static_cast<double>(GcdMs) / 1000.0;
        bGCDUsesServerClock = true;
        GCDEndTime = GetServerSeconds() + GcdSec;
        UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Started GCD %.1fs"), GcdSec);
    }
}

void UPlayerSkillManager::NotifyAnimationEnded()
{
    UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Animation ended, cast lock released"));
    // TODO: release cast lock if one is implemented
}

double UPlayerSkillManager::GetServerSeconds() const {
    if (TimeSyncService && TimeSyncService->IsTimeSyncValid()) {
        return static_cast<double>(TimeSyncService->GetEstimatedServerTimeMs()) / 1000.0;
    }
    // ������: ��������� world->time � "���������" ����� ���������� offset, ���� ����
    if (UWorld* W = GetWorld()) return static_cast<double>(W->GetTimeSeconds());
    return 0.0;
}

double UPlayerSkillManager::GetWorldSeconds() const {
    if (UWorld* W = GetWorld()) return static_cast<double>(W->GetTimeSeconds());
    return 0.0;
}

double UPlayerSkillManager::NowFor(const FPlayerSkillData* Skill) const {
    if (Skill && Skill->bCooldownUsesServerClock) return GetServerSeconds();
    return GetWorldSeconds();
}

bool UPlayerSkillManager::IsGCDActive() const
{
    const double Now = bGCDUsesServerClock ? GetServerSeconds() : GetWorldSeconds();
    return Now < GCDEndTime;
}

float UPlayerSkillManager::GetGCDRemaining() const
{
    const double Now = bGCDUsesServerClock ? GetServerSeconds() : GetWorldSeconds();
    const double Rem = GCDEndTime - Now;
    return Rem > 0.0 ? static_cast<float>(Rem) : 0.0f;
}