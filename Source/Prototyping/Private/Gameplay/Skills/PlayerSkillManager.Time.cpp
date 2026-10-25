#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Prototyping.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"

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

void UPlayerSkillManager::ConvertServerClockCooldownsToWorldTime()
{
    const double WorldNow = GetWorldSeconds();

    for (auto& Pair : PlayerSkills)
    {
        FPlayerSkillData& S = Pair.Value;
        if (!S.bIsOnCooldown || !S.bCooldownUsesServerClock) continue;

        // GetServerSeconds() now returns the world-time fallback (time-sync invalid),
        // so remaining = endTime - worldTime gives a correct relative duration.
        const double ServerNow = GetServerSeconds();
        double RemainingSec = S.cooldownEndTime - ServerNow;
        if (RemainingSec <= 0.0)
        {
            RemainingSec = 0.01; // expire next tick
        }

        S.cooldownEndTime = WorldNow + RemainingSec;
        S.bCooldownUsesServerClock = false;

        UE_LOG(LogGameplay, Warning,
            TEXT("PlayerSkillManager: Converted cooldown '%s' to world-time — remaining=%.1fs newEnd=%.3f"),
            *Pair.Key, RemainingSec, S.cooldownEndTime);
    }

    if (bGCDUsesServerClock)
    {
        const double ServerNow = GetServerSeconds();
        double RemainingSec = GCDEndTime - ServerNow;
        if (RemainingSec <= 0.0) RemainingSec = 0.01;
        GCDEndTime = WorldNow + RemainingSec;
        bGCDUsesServerClock = false;

        UE_LOG(LogGameplay, Warning,
            TEXT("PlayerSkillManager: Converted GCD to world-time — remaining=%.1fs"), RemainingSec);
    }

    UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Server-clock cooldowns converted to world-time (time-sync lost)"));
}
