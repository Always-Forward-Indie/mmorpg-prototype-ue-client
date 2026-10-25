// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Prototyping.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

bool UPlayerAnimInstance::HasCastReleaseSection(const UAnimMontage* Montage)
{
    if (!Montage) return false;
    return Montage->GetSectionIndex(FName("CastRelease")) != INDEX_NONE;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

UAnimMontage* UPlayerAnimInstance::ResolveMontage(const FName& AnimationName) const
{
    if (const UAnimMontage* const* Found = AttackMontageMap.Find(AnimationName))
    {
        if (*Found) return const_cast<UAnimMontage*>(*Found);
    }
    return DefaultAttackMontage;
}

float UPlayerAnimInstance::CalcPlayRate(const UAnimMontage* Montage, float DesiredDuration)
{
    if (!Montage || DesiredDuration <= 0.0f) return 1.0f;

    const float MontageLength = Montage->GetPlayLength();
    if (MontageLength <= 0.0f) return 1.0f;

    // Clamp to a sane range so we never play absurdly fast or slow
    return FMath::Clamp(MontageLength / DesiredDuration, 0.5f, 3.0f);
}

float UPlayerAnimInstance::CalcHitDelay(const UAnimMontage* Montage, float PlayRate, float ActualDuration)
{
    if (Montage && PlayRate > 0.0f)
    {
        // Walk every notify track looking for "HitPoint" or "Attack_Hit"
        for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
        {
            const FName NotifyName = NotifyEvent.NotifyName;
            if (NotifyName == FName("HitPoint") || NotifyName == FName("Attack_Hit"))
            {
                // TriggerTime is in montage-seconds (at PlayRate=1).
                // Divide by PlayRate to get wall-clock seconds.
                const float WallClockTime = NotifyEvent.GetTriggerTime() / PlayRate;
                UE_LOG(LogGameplay, Verbose,
                    TEXT("[PlayerAnim] HitDelay from notify '%s': montageTime=%.3fs playRate=%.3f wallClock=%.3fs"),
                    *NotifyName.ToString(), NotifyEvent.GetTriggerTime(), PlayRate, WallClockTime);
                return WallClockTime;
            }
        }
        UE_LOG(LogGameplay, Verbose,
            TEXT("[PlayerAnim] No HitPoint notify found in montage '%s', using DefaultHitRatio fallback"),
            *Montage->GetName());
    }
    return ActualDuration * DefaultHitRatio;
}

void UPlayerAnimInstance::OnAttackMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
{
    // If the montage was interrupted but a new attack is already in progress
    // (StartAttack set bIsAttacking=true before Montage_Play triggered this callback),
    // do NOT reset state — the new attack owns it now.
    if (bInterrupted && bIsAttacking)
    {
        return;
    }

    const int32 EndedCasterId = CurrentCasterId;

    bIsAttacking      = false;
    CurrentAttackSlot = NAME_None;
    CurrentCasterId   = 0;
    ActiveCastMontage = nullptr;

    // Ensure pending timers are cleared if they somehow didn't fire
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitPointTimerHandle);
        World->GetTimerManager().ClearTimer(CastReleaseTimerHandle);
    }

    // If the montage was interrupted (e.g. by death or new skill) and the hit-point
    // never fired, flush any pending results now so damage doesn't wait 12 seconds.
    if (bInterrupted && EndedCasterId > 0)
    {
        OnHitPoint.Broadcast(EndedCasterId);
    }

    // Notify listeners (e.g. PlayerSkillManager) that the animation is done
    OnAttackEnded.Broadcast();
}
