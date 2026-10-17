#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"

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

