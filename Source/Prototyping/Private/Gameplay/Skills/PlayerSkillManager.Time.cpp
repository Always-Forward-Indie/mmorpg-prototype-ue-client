#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Prototyping.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"
#include "Utils/NetCore/CooldownFormat.h"

double UPlayerSkillManager::GetWorldSeconds() const {
    if (UWorld* W = GetWorld()) return static_cast<double>(W->GetTimeSeconds());
    return 0.0;
}

bool UPlayerSkillManager::IsGCDActive() const
{
    return GetWorldSeconds() < GCDEndTime;
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
    const double Rem = GCDEndTime - GetWorldSeconds();
    if (Rem <= 0.0) return 0.0f;
    // Bug 8 fix: clamp GCD remaining to a sane maximum (no GCD should exceed 10 s).
    // Cap owned by FCooldownAnchor (single source with the travel carry cap).
    return FMath::Min(static_cast<float>(Rem), static_cast<float>(FCooldownAnchor::MaxGcdCarrySec));
}

void UPlayerSkillManager::BeginTravelCooldownSnapshot()
{
    // World clocks reset on OpenLevel. Snapshot remaining seconds now (old
    // world still ticking) so ApplyTravelCooldownSnapshot can re-anchor them
    // to the new world instead of comparing old-world endTimes to new times
    // (which reads as "still on cooldown" forever).
    TravelCooldownRemaining.Empty();
    const double Now = GetWorldSeconds();
    for (const auto& Pair : PlayerSkills)
    {
        const FPlayerSkillData& S = Pair.Value;
        if (S.bIsOnCooldown)
        {
            TravelCooldownRemaining.Add(Pair.Key, FMath::Max(0.0, S.cooldownEndTime - Now));
        }
    }
    TravelGCDRemaining = FMath::Max(0.0, GCDEndTime - Now);
    bHasTravelSnapshot = true;
}

void UPlayerSkillManager::ApplyTravelCooldownSnapshot()
{
    if (!bHasTravelSnapshot) return;
    bHasTravelSnapshot = false;

    const double Now = GetWorldSeconds();
    for (auto& Pair : PlayerSkills)
    {
        if (const double* Remaining = TravelCooldownRemaining.Find(Pair.Key))
        {
            FPlayerSkillData& S = Pair.Value;
            // Cap at the skill's own base duration: a remaining can never
            // legitimately exceed it, so anything larger is clock garbage.
            // Dropped entries are reset to the fresh state (never left with
            // an old-world endTime, which would read as stuck).
            const double MaxSec = S.networkData.cooldownMs > 0
                ? static_cast<double>(S.networkData.cooldownMs) / 1000.0
                : 300.0;
            if (FCooldownAnchor::Reanchor(Now, *Remaining, MaxSec, S.cooldownEndTime))
            {
                S.bIsOnCooldown = true;
                S.bIsReady = false;
            }
            else
            {
                S.cooldownEndTime = Now;
                S.bIsOnCooldown = false;
                S.bIsReady = true;
            }
        }
    }
    TravelCooldownRemaining.Empty();
    // Same rule for GCD: drop instead of re-arming garbage. EndTime stays in
    // the new clock base either way (never an old-world residue).
    if (!FCooldownAnchor::Reanchor(Now, TravelGCDRemaining, FCooldownAnchor::MaxGcdCarrySec, GCDEndTime))
    {
        GCDEndTime = Now;
    }
    TravelGCDRemaining = 0.0;
    // No re-broadcast needed: SkillSlotWidget ticks IsSkillOnCooldown/
    // GetSkillCooldownRemaining every frame and picks the new state up.
}
