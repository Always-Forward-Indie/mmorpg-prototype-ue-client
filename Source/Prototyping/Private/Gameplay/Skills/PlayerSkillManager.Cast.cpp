#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Prototyping.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"
#include "Utils/NetCore/CooldownFormat.h"

bool UPlayerSkillManager::TryCastSkill(const FString& SkillSlug, int32 TargetId, ECasterType TargetType)
{
    if (!CanCastSkill(SkillSlug))
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Cannot cast skill %s"), *SkillSlug);
        return false;
    }

    // Resolve and validate the target based on skill target policy
    int32 ResolvedTargetId = TargetId;
    ECasterType ResolvedTargetType = TargetType;
    if (!ResolveSkillTarget(SkillSlug, ResolvedTargetId, ResolvedTargetType))
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Invalid target for skill %s (targetId=%d, targetType=%d)"),
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
            UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillManager: Sent skill request %s (target=%d, type=%d) - waiting for server confirmation"),
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
        // Predicate lives in FCooldownState (pinned by MMO.Skills.CooldownState).
        return FCooldownState::IsOnCooldown(
            static_cast<float>(GetWorldSeconds()), static_cast<float>(S->cooldownEndTime));
    }
    return false;
}

float UPlayerSkillManager::GetSkillCooldownRemaining(const FString& SkillSlug) const
{
    if (const FPlayerSkillData* S = PlayerSkills.Find(SkillSlug)) {
        const double now = GetWorldSeconds();
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
        if (dur <= 0.0) { UE_LOG(LogGameplay, Error, TEXT("Invalid cooldown")); return; }

        // Single world-time clock (see HandleSkillInitiation). No server-epoch
        // math: mixing epochs produced permanent cooldowns on sync loss.
        const double now = GetWorldSeconds();
        S->cooldownEndTime = now + dur;
        S->bIsOnCooldown = true;
        S->bIsReady = false;

        OnSkillCooldownStarted.Broadcast(SkillSlug);
    }
}

