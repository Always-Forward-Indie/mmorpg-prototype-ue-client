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

    // Check animation cast lock: a previous non-auto-attack animation is still playing.
    // Block the cast so the new animation does not interrupt the pending result flush.
    // The lock auto-expires after AnimationLockTimeoutSec in case NotifyAnimationEnded
    // never fires (missing montage end notify).
    if (bIsAnimationPlaying)
    {
        const double Elapsed = GetWorldSeconds() - AnimationStartWorldTime;
        if (Elapsed < AnimationLockTimeoutSec)
        {
            UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Cannot cast %s - animation lock active (%.1fs elapsed)"),
                *SkillSlug, Elapsed);
            return false;
        }
        // Lock timed out — clear and allow the cast
        const_cast<UPlayerSkillManager*>(this)->bIsAnimationPlaying = false;
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Animation lock timed out for %s"), *SkillSlug);
    }

    // Check per-skill server-confirmation guard: a request for THIS specific
    // skill was sent but combatInitiation hasn't arrived yet.  Other skills can
    // still be cast during each other's round-trip window.
    if (const double* ReqTime = PendingConfirmations.Find(SkillSlug))
    {
        const double Elapsed = GetWorldSeconds() - *ReqTime;
        const double Timeout = GetConfirmationTimeout();
        if (Elapsed < Timeout)
        {
            UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Cannot cast %s - awaiting server confirmation (%.1fs/%.1fs elapsed)"),
                *SkillSlug, Elapsed, Timeout);
            return false;
        }
        // Server never replied — auto-expire
        const_cast<UPlayerSkillManager*>(this)->PendingConfirmations.Remove(SkillSlug);
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Server confirmation timed out for %s after %.1fs (timeout=%.1fs)"),
            *SkillSlug, Elapsed, Timeout);
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

    // Resolve and validate the target based on skill target policy
    int32 ResolvedTargetId = TargetId;
    ECasterType ResolvedTargetType = TargetType;
    if (!ResolveSkillTarget(SkillSlug, ResolvedTargetId, ResolvedTargetType))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Invalid target for skill %s (targetId=%d, targetType=%d)"),
            *SkillSlug, TargetId, static_cast<int32>(TargetType));
        return false;
    }

    // Set the per-skill confirmation guard BEFORE sending the request.
    PendingConfirmations.Add(SkillSlug, GetWorldSeconds());

    // Cast skill through existing system - DON'T start cooldown here anymore
    if (SkillSystemManager)
    {
        bool bSuccess = SkillSystemManager->CastSkill(CharacterId, ResolvedTargetId, SkillSlug, ResolvedTargetType);
        
        if (bSuccess)
        {
            UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Sent skill request %s (target=%d, type=%d) - waiting for server confirmation"),
                *SkillSlug, ResolvedTargetId, static_cast<int32>(ResolvedTargetType));
        }
        else
        {
            // Send failed — release the guard so the player can retry.
            PendingConfirmations.Remove(SkillSlug);
        }
        
        return bSuccess;
    }

    PendingConfirmations.Remove(SkillSlug);
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
        if (rem <= 0.0) return 0.0f;
        // Bug 8 fix: clamp to the skill's maximum cooldown duration so that a loss
        // of TimeSyncService sync (which causes a massive time-base mismatch between
        // the stored server-absolute cooldownEndTime and the world-time fallback) never
        // results in absurdly large "remaining" values on the skill bar.
        const float MaxCooldownSec = S->networkData.cooldownMs > 0
            ? S->networkData.cooldownMs / 1000.0f
            : 300.0f; // hard cap: no skill cooldown should ever display > 5 minutes
        return FMath::Min(static_cast<float>(rem), MaxCooldownSec);
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

    // Defense: a successful initiation should carry a non-zero cooldown or GCD.
    // Error initiations (rejected by server) have both at zero and are filtered
    // by CombatNetworkHandler. This guard is a safety-net against a missing filter.
    if (CooldownMs <= 0 && GcdMs <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillManager: Suspicious initiation for %s - cooldownMs=%d gcdMs=%d. Processing anyway but it may be an error."),
            *SkillSlug, CooldownMs, GcdMs);
    }

    // Server confirmed the cast — release the per-skill confirmation guard.
    PendingConfirmations.Remove(SkillSlug);
    OnSkillCastConfirmed.Broadcast(SkillSlug, CooldownMs);

    // Set animation cast lock so that re-casting the same skill while the
    // animation is still playing is blocked.  The lock is cleared by
    // NotifyAnimationEnded (montage end) or auto-expires after AnimationLockTimeoutSec.
    // This was previously skipped for basic_attack but that allowed auto-attack to
    // send a new combatInitiation while the animation was still playing, causing
    // StartAttack's bIsAttacking guard to silently reject the new montage and lose
    // the HitPoint timer — resulting in damage queued until the next Init.
    bIsAnimationPlaying     = true;
    AnimationStartWorldTime = GetWorldSeconds();
    
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

void UPlayerSkillManager::NotifyAnimationEnded(){
    bIsAnimationPlaying      = false;
    UE_LOG(LogTemp, Log, TEXT("PlayerSkillManager: Animation ended, cast lock released"));
}

void UPlayerSkillManager::ApplyServerCooldowns(const TArray<FSkillCooldownEntry>& Cooldowns)
{
    // IMPORTANT: bCooldownUsesServerClock must match the time source used to set
    // cooldownEndTime. If we set endTime = worldTime+X but mark the skill as using
    // server clock, IsSkillOnCooldown will compare serverTime (~1.7 billion) against
    // worldTime+X (~67) and always return false — cooldown appears instantly expired.
    // See also: Bug 8 comment in GetSkillCooldownRemaining.
    const bool bServerSyncValid = TimeSyncService && TimeSyncService->IsTimeSyncValid();
    const double Now = bServerSyncValid
        ? static_cast<double>(TimeSyncService->GetEstimatedServerTimeMs()) / 1000.0
        : GetWorldSeconds();

    UE_LOG(LogTemp, Warning,
        TEXT("PlayerSkillManager: ApplyServerCooldowns — syncValid=%d, now=%.3f, entries=%d"),
        bServerSyncValid ? 1 : 0, Now, Cooldowns.Num());

    int32 Applied = 0;

    for (const FSkillCooldownEntry& Entry : Cooldowns)
    {
        if (Entry.remainingMs <= 0)
            continue; // already expired server-side — nothing to restore

        FPlayerSkillData* S = PlayerSkills.Find(Entry.skillSlug);
        if (!S)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("PlayerSkillManager: ApplyServerCooldowns - unknown skill '%s', skipping"),
                *Entry.skillSlug);
            continue;
        }

        // Sanity clamp: remainingMs must not exceed the skill's base cooldown.
        // Guards against server bugs / extreme clock drift permanently freezing a skill.
        const int32 MaxMs = S->networkData.cooldownMs > 0 ? S->networkData.cooldownMs : 300000;
        const int32 ClampedMs = FMath::Clamp(Entry.remainingMs, 1, MaxMs);

        const double RemainingSec = static_cast<double>(ClampedMs) / 1000.0;
        // Use the same clock that was used to compute Now — keeps set/check consistent.
        S->bCooldownUsesServerClock = bServerSyncValid;
        S->cooldownEndTime          = Now + RemainingSec;
        S->bIsOnCooldown            = true;
        S->bIsReady                 = false;

        OnSkillCooldownStarted.Broadcast(Entry.skillSlug);

        UE_LOG(LogTemp, Warning,
            TEXT("PlayerSkillManager: Restored cooldown %.1fs for '%s' (clamped from %dms to %dms) endTime=%.3f useSrvClock=%d"),
            RemainingSec, *Entry.skillSlug, Entry.remainingMs, ClampedMs, S->cooldownEndTime, bServerSyncValid ? 1 : 0);
        ++Applied;
    }

    UE_LOG(LogTemp, Log,
        TEXT("PlayerSkillManager: ApplyServerCooldowns — applied %d / %d entries"),
        Applied, Cooldowns.Num());
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

bool UPlayerSkillManager::IsSkillAnimationPlaying() const
{
    if (!bIsAnimationPlaying) return false;
    // Auto-expire check (mirrors CanCastSkill logic)
    return (GetWorldSeconds() - AnimationStartWorldTime) < AnimationLockTimeoutSec;
}

double UPlayerSkillManager::GetConfirmationTimeout() const
{
    if (HasReliableTimeSync())
    {
        const FTimeSyncData SyncData = TimeSyncService->GetCurrentTimeSyncData(EServerType::ChunkServer);
        const float RTT = SyncData.RoundTripTimeMs;
        // 5x RTT (in seconds), clamped to [Min, Max]
        const double RTTBased = static_cast<double>(RTT) * 5.0 / 1000.0;
        return FMath::Clamp(RTTBased, MinConfirmationTimeoutSec, MaxConfirmationTimeoutSec);
    }
    return MinConfirmationTimeoutSec;
}

bool UPlayerSkillManager::HasReliableTimeSync() const
{
    if (!TimeSyncService || !TimeSyncService->IsTimeSyncValid())
    {
        return false;
    }
    const FTimeSyncData SyncData = TimeSyncService->GetCurrentTimeSyncData(EServerType::ChunkServer);
    return SyncData.RoundTripTimeMs > 0.0f;
}

float UPlayerSkillManager::GetGCDRemaining() const
{
    const double Now = bGCDUsesServerClock ? GetServerSeconds() : GetWorldSeconds();
    const double Rem = GCDEndTime - Now;
    if (Rem <= 0.0) return 0.0f;
    // Bug 8 fix: clamp GCD remaining to a sane maximum (no GCD should exceed 10 s).
    return FMath::Min(static_cast<float>(Rem), 10.0f);
}