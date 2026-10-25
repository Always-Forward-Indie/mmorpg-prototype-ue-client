// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Prototyping.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

// ---------------------------------------------------------------------------
void UPlayerAnimInstance::StartAttack(const FSkillInitiationData& SkillData)
{
    if (bIsDead) return;

    // A new combatInitiation arrived while an animation is still playing.
    // Flush the current animation's pending results now (HitPoint will never fire)
    // and stop the old montage so the new one can start cleanly.
    if (bIsAttacking)
    {
        UE_LOG(LogGameplay, Warning,
            TEXT("[PlayerAnim] StartAttack superseding current animation (slot='%s' -> '%s')"),
            *CurrentAttackSlot.ToString(), *SkillData.animationName);

        // Flush pending results for the superseded animation's caster
        if (CurrentCasterId > 0)
        {
            OnHitPoint.Broadcast(CurrentCasterId);
        }

        // Stop the old montage
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(HitPointTimerHandle);
            World->GetTimerManager().ClearTimer(CastReleaseTimerHandle);
        }
        if (ActiveCastMontage.IsValid())
        {
            Montage_Stop(0.0f, ActiveCastMontage.Get());
            ActiveCastMontage = nullptr;
        }

        bIsAttacking      = false;
        CurrentAttackSlot = NAME_None;
        CurrentCasterId   = 0;
    }

    bIsAttacking      = true;
    bIsAggressive     = true;
    CurrentAttackSlot = FName(*SkillData.animationName);
    CurrentCasterId   = SkillData.casterId;

    // Cancel any previous hit-point timer
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitPointTimerHandle);
    }

    UAnimMontage* Montage = ResolveMontage(CurrentAttackSlot);

    UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] StartAttack: slot='%s' duration=%.3fs montage=%s"),
        *CurrentAttackSlot.ToString(), SkillData.animationDuration,
        Montage ? *Montage->GetName() : TEXT("nullptr (FALLBACK)"));

    if (Montage)
    {
        const float CastTime = SkillData.castTime; // 0 for instant skills

        // ---------------------------------------------------------------------------
        // Phase-based path: cast-time skills whose montage has a "CastRelease" section.
        //
        // The montage is divided into two phases at runtime:
        //   Phase 1 (CastLoop):    plays at CastLoopRate, loops until T=CastTime.
        //   Phase 2 (CastRelease): plays at 1.0x, jumped-to at T=CastTime.
        //
        // The CastRelease anim-notify fires immediately after the jump, which is at
        // exactly T=CastTime — matching the server's combatResult delivery window.
        // The EarlyProjectileImpact mechanism handles the remaining ±100ms tolerance.
        // ---------------------------------------------------------------------------
        if (CastTime > 0.0f && HasCastReleaseSection(Montage))
        {
            // Auto-calculate the CastLoop playback rate so the Default section
            // completes exactly once in CastTime seconds (no looping needed).
            // Rate = (length of Default portion) / castTime.
            // Example: Default portion = 1.5s, castTime = 4.0s → rate = 0.375x
            // The animation plays once at reduced speed — a natural slow charge-up.
            const int32 ReleaseIdx = Montage->GetSectionIndex(FName("CastRelease"));
            float ReleaseSectionStart = 0.0f, ReleaseSectionEnd = 0.0f;
            Montage->GetSectionStartAndEndTime(ReleaseIdx, ReleaseSectionStart, ReleaseSectionEnd);
            const float CastLoopLength = ReleaseSectionStart; // Default section spans [0, ReleaseSectionStart)

            const float AutoRate = (CastLoopLength > 0.001f && CastTime > 0.001f)
                ? FMath::Clamp(CastLoopLength / CastTime, 0.05f, 1.0f)
                : 1.0f;

            CurrentAttackPlayRate = AutoRate;
            ActiveCastMontage    = Montage;

            UE_LOG(LogGameplay, Warning,
                TEXT("[PlayerAnim] CastRelease section found — castTime=%.3fs loopLen=%.3fs autoRate=%.3f"),
                CastTime, CastLoopLength, AutoRate);

            Montage_Play(Montage, AutoRate);
            Montage_SetEndDelegate(MontageEndedDelegate, Montage);

            // Timer fires at T=castTime to ensure we enter CastRelease at 1.0x.
            // Normally the montage transitions naturally (Default→CastRelease) at the
            // same moment since AutoRate was calculated to fill exactly castTime.
            // The timer is a safety net for float imprecision and also switches rate.
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().ClearTimer(CastReleaseTimerHandle);
                World->GetTimerManager().SetTimer(CastReleaseTimerHandle, this,
                    &UPlayerAnimInstance::OnCastReleaseTimer, CastTime, false);
            }
        }
        // ---------------------------------------------------------------------------
        // Classic path: instant skills (castTime == 0) or montages without a
        // "CastRelease" section.  Stretches the whole montage to animationDuration.
        // ---------------------------------------------------------------------------
        else
        {
            const float PlayRate = CalcPlayRate(Montage, SkillData.animationDuration);
            CurrentAttackPlayRate = PlayRate;

            const float MontageLength  = Montage->GetPlayLength();
            const float ActualDuration = (PlayRate > 0.0f) ? MontageLength / PlayRate : SkillData.animationDuration;
            const float HitDelay       = CalcHitDelay(Montage, PlayRate, ActualDuration);

            UE_LOG(LogGameplay, Warning,
                TEXT("[PlayerAnim] Montage: length=%.3fs playRate=%.3f actualDuration=%.3fs hitDelay=%.3fs"),
                MontageLength, PlayRate, ActualDuration, HitDelay);

            Montage_Play(Montage, PlayRate);
            Montage_SetEndDelegate(MontageEndedDelegate, Montage);

            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(HitPointTimerHandle, this,
                    &UPlayerAnimInstance::FireHitPoint, HitDelay, false);
            }
        }
    }
    else
    {
        // No montage assigned yet � fall back to a timer so bIsAttacking is still cleared
        CurrentAttackPlayRate = 1.0f;
        const float Duration = FMath::Max(SkillData.animationDuration, 0.1f);
        const float HitDelay = Duration * DefaultHitRatio;

        UE_LOG(LogGameplay, Warning,
            TEXT("[PlayerAnim] No montage for slot '%s'. Maps: %d entries, DefaultAttackMontage=%s"),
            *CurrentAttackSlot.ToString(), AttackMontageMap.Num(),
            DefaultAttackMontage ? *DefaultAttackMontage->GetName() : TEXT("nullptr"));
        UE_LOG(LogGameplay, Warning,
            TEXT("[PlayerAnim] FALLBACK timer: duration=%.3fs hitDelay=%.3fs"),
            Duration, HitDelay);

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(HitPointTimerHandle, this,
                &UPlayerAnimInstance::FireHitPoint, HitDelay, false);

            FTimerHandle EndHandle;
            World->GetTimerManager().SetTimer(EndHandle, [this]()
            {
                bIsAttacking      = false;
                CurrentAttackSlot = NAME_None;
                CurrentCasterId   = 0;
            }, Duration, false);
        }
    }
}

// ---------------------------------------------------------------------------
// FireHitPoint � broadcasts OnHitPoint at the visual moment of impact.
// Called either by UAnimNotify_HitPoint (montage timeline) or the fallback timer.
// Clears the fallback timer so only one call goes through per attack.
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::FireHitPoint()
{
    // Cancel the fallback timer � in case the Notify fired first
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitPointTimerHandle);
    }

    UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] FireHitPoint: casterId=%d listeners=%d"),
        CurrentCasterId, OnHitPoint.IsBound() ? 1 : 0);

    if (CurrentCasterId > 0)
    {
        OnHitPoint.Broadcast(CurrentCasterId);
    }
}
// ---------------------------------------------------------------------------
// OnCastReleaseTimer
//   Fired at T=castTime by CastReleaseTimerHandle.
//   Jumps the active montage to the "CastRelease" section and switches to
//   natural (1.0x) playback so the release animation plays at editor speed.
//   The CastRelease anim-notify inside that section fires on the same tick,
//   triggering PlayCombatSoundEvent(CastRelease) which spawns the projectile.
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::OnCastReleaseTimer()
{
    UAnimMontage* Montage = ActiveCastMontage.Get();
    if (!Montage)
    {
        UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] OnCastReleaseTimer: active montage is gone, skipping"));
        return;
    }

    // The montage should have naturally transitioned Default→CastRelease by now
    // (AutoRate was calculated to finish the Default section at exactly T=castTime).
    // Only force-jump if float imprecision left us still in Default.
    const FName CurrentSection = Montage_GetCurrentSection(Montage);
    if (CurrentSection != FName("CastRelease"))
    {
        UE_LOG(LogGameplay, Warning,
            TEXT("[PlayerAnim] OnCastReleaseTimer: still in section '%s', forcing JumpToSection"),
            *CurrentSection.ToString());
        Montage_JumpToSection(FName("CastRelease"), Montage);
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] OnCastReleaseTimer: in CastRelease, setting rate=1.0"));
    }

    // Ensure CastRelease always plays at the animator's intended 1.0x speed.
    Montage_SetPlayRate(Montage, 1.0f);
    CurrentAttackPlayRate = 1.0f;
}

// ---------------------------------------------------------------------------
// StopCurrentAttack
//   Blends out of the active attack montage so the character can transition
//   to locomotion without foot-sliding.  Called from BasicPlayer::Move() when
//   the player starts moving after the cast bar has ended but before the
//   release animation has fully played out.
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::StopCurrentAttack(float BlendOutTime)
{
    if (!bIsAttacking) return;

    // Clear any pending timers so they don't fire after the blend
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitPointTimerHandle);
        World->GetTimerManager().ClearTimer(CastReleaseTimerHandle);
    }

    // Blend out of the active montage.
    // The OnAttackMontageEnded delegate handles the rest: clearing state,
    // flushing pending results if needed, and broadcasting OnAttackEnded.
    if (ActiveCastMontage.IsValid())
    {
        Montage_Stop(BlendOutTime, ActiveCastMontage.Get());
    }
    else
    {
        Montage_Stop(BlendOutTime);
    }
}

// ---------------------------------------------------------------------------
// NotifyDeath
//   Latch bIsDead, stop any active montage, let the State Machine take over.
// ---------------------------------------------------------------------------
