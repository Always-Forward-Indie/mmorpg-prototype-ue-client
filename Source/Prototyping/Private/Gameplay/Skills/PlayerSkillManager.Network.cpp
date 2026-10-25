#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Prototyping.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"

void UPlayerSkillManager::HandleSkillInitiation(const FString& SkillSlug, int32 CasterId,
    int32 CooldownMs, int32 GcdMs)
{
    // Only handle initiations for our character
    if (CasterId != CharacterId)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillManager: Ignoring skill initiation for other character %d (ours: %d)"), 
            CasterId, CharacterId);
        return;
    }
    
    UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Server-confirmed skill %s (cooldownMs=%d, gcdMs=%d)"),
        *SkillSlug, CooldownMs, GcdMs);

    // Defense: a successful initiation should carry a non-zero cooldown or GCD.
    // Error initiations (rejected by server) have both at zero and are filtered
    // by CombatNetworkHandler. This guard is a safety-net against a missing filter.
    if (CooldownMs <= 0 && GcdMs <= 0)
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Suspicious initiation for %s - cooldownMs=%d gcdMs=%d. Processing anyway but it may be an error."),
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
                const bool bSyncValid = TimeSyncService && TimeSyncService->IsTimeSyncValid();
                S->bCooldownUsesServerClock = bSyncValid;
                S->cooldownEndTime = (bSyncValid ? GetServerSeconds() : GetWorldSeconds()) + CooldownSec;
                S->bIsOnCooldown = true;
                S->bIsReady = false;

                OnSkillCooldownStarted.Broadcast(SkillSlug);
                UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillManager: Started cooldown %.1fs for %s"), CooldownSec, *SkillSlug);
            }
        }
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Server confirmed skill %s but we don't have it"), *SkillSlug);
    }

    // Start GCD (affects all skills)
    if (GcdMs > 0)
    {
        const double GcdSec = static_cast<double>(GcdMs) / 1000.0;
        bGCDUsesServerClock = false;
        GCDEndTime = GetWorldSeconds() + GcdSec;
        UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillManager: Started GCD %.1fs"), GcdSec);
    }
}

void UPlayerSkillManager::NotifyAnimationEnded(){
    bIsAnimationPlaying      = false;
    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillManager: Animation ended, cast lock released"));
}

void UPlayerSkillManager::ApplyServerCooldowns(const TArray<FSkillCooldownEntry>& Cooldowns)
{
    // Use world time for cooldown restoration.  remainingMs is a relative duration,
    // so server-epoch anchoring adds no value and creates a clock-mismatch risk when
    // time sync drops out after relogin (world-time ~67 vs server-epoch ~1.75B).
    const double Now = GetWorldSeconds();

    UE_LOG(LogGameplay, Warning,
        TEXT("PlayerSkillManager: ApplyServerCooldowns — now=%.3f, entries=%d"),
        Now, Cooldowns.Num());

    int32 Applied = 0;

    for (const FSkillCooldownEntry& Entry : Cooldowns)
    {
        if (Entry.remainingMs <= 0)
            continue; // already expired server-side — nothing to restore

        FPlayerSkillData* S = PlayerSkills.Find(Entry.skillSlug);
        if (!S)
        {
            UE_LOG(LogGameplay, Warning,
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
        S->bCooldownUsesServerClock = false;
        S->cooldownEndTime          = Now + RemainingSec;
        S->bIsOnCooldown            = true;
        S->bIsReady                 = false;

        OnSkillCooldownStarted.Broadcast(Entry.skillSlug);

        UE_LOG(LogGameplay, Warning,
            TEXT("PlayerSkillManager: Restored cooldown %.1fs for '%s' (clamped from %dms to %dms) endTime=%.3f"),
            RemainingSec, *Entry.skillSlug, Entry.remainingMs, ClampedMs, S->cooldownEndTime);
        ++Applied;
    }

    UE_LOG(LogGameplay, Verbose,
        TEXT("PlayerSkillManager: ApplyServerCooldowns — applied %d / %d entries"),
        Applied, Cooldowns.Num());
}

