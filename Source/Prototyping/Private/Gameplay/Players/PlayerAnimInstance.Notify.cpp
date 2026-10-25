// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Prototyping.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

void UPlayerAnimInstance::NotifyDeath()
{
    bIsDead           = true;
    bIsAttacking      = false;
    bIsAggressive     = false;
    bIsHit            = false;
    CurrentAttackSlot = NAME_None;

    // Stop any playing attack/hit montages so the Death state in the
    // State Machine can take over cleanly without being overridden by a slot
    Montage_StopGroupByName(0.1f, FName("DefaultGroup"));
    // bIsDead=true causes the State Machine to transition to the Death state
    // which plays A_Death as a regular Sequence (Loop=false, freeze on last frame)
}

// ---------------------------------------------------------------------------
// NotifyRevive
//   Clear the death state so the State Machine transitions back to alive states.
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::NotifyRevive()
{
    bIsDead = false;
}

// ---------------------------------------------------------------------------
// NotifyTargetLost
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::NotifyTargetLost()
{
    bIsAggressive     = false;
    bIsAttacking      = false;
    CurrentAttackSlot = NAME_None;

    // Blend out attack montage if one is playing
    Montage_StopGroupByName(0.2f, FName("DefaultSlot"));
}

// ---------------------------------------------------------------------------
// NotifyHit  (combatResult � this player is the TARGET)
//   Plays a hit-react montage over the current locomotion/combat-idle.
//   Clears bIsHit automatically when the montage ends.
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::NotifyHit()
{
    if (bIsDead) return;
    // Never interrupt an active cast or attack animation with a hit-react.
    // The hit will be visible via HP bar / floating text.
    if (bIsAttacking) return;

    bIsHit = true;

    if (HitReactMontage)
    {
        Montage_Play(HitReactMontage, 1.0f);

        // Bind a one-shot lambda to clear bIsHit when this montage finishes
        FOnMontageEnded HitEndDelegate;
        HitEndDelegate.BindLambda([this](UAnimMontage*, bool)
        {
            bIsHit = false;
        });
        Montage_SetEndDelegate(HitEndDelegate, HitReactMontage);
    }
    else
    {
        // No montage � clear after a fixed window
        if (UWorld* World = GetWorld())
        {
            FTimerHandle Handle;
            World->GetTimerManager().SetTimer(Handle, [this]()
            {
                bIsHit = false;
            }, 0.3f, false);
        }
    }
}

// ---------------------------------------------------------------------------
// NotifyPickup � plays the pickup montage and returns its duration.
// A fallback timer fires FirePickupPoint() at ~50 % of the montage so that
// the DroppedItemActor is removed even when no AnimNotify_PickupPoint is
// placed on the timeline.
// ---------------------------------------------------------------------------
float UPlayerAnimInstance::NotifyPickup()
{
    if (!PickupMontage)
    {
        UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] NotifyPickup: PickupMontage is NULL - skipping montage"));
        return 0.0f;
    }

    const float PlayRate     = 1.0f;
    const float MontageLen   = PickupMontage->GetPlayLength();
    const float Duration     = MontageLen / PlayRate;
    const float PickupDelay  = Duration * 0.5f;

    UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] NotifyPickup: playing montage '%s' len=%.3fs delay=%.3fs"),
        *PickupMontage->GetName(), MontageLen, PickupDelay);

    // Cancel any previous pickup-point timer
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PickupPointTimerHandle);
    }

    Montage_Play(PickupMontage, PlayRate);

    // Set fallback timer � AnimNotify_PickupPoint will cancel it if present
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(PickupPointTimerHandle, this,
            &UPlayerAnimInstance::FirePickupPoint, PickupDelay, false);
    }

    return Duration;
}

float UPlayerAnimInstance::NotifyHarvest(float DesiredDurationSeconds)
{
    if (!HarvestMontage)
    {
        return 0.0f;
    }

    const float MontageLen = HarvestMontage->GetPlayLength();
    const float PlayRate = (MontageLen > 0.001f && DesiredDurationSeconds > 0.001f)
        ? FMath::Clamp(MontageLen / DesiredDurationSeconds, 0.5f, 3.0f)
        : 1.0f;

    UE_LOG(LogGameplay, Verbose, TEXT("[PlayerAnim] NotifyHarvest: playing montage '%s' len=%.3fs desired=%.1fs rate=%.2f"),
        *HarvestMontage->GetName(), MontageLen, DesiredDurationSeconds, PlayRate);

    Montage_Play(HarvestMontage, PlayRate);
    return MontageLen / PlayRate;
}

// ---------------------------------------------------------------------------
// CancelPickupTimer � cancels the fallback pickup-point timer without
// broadcasting. Used by ItemManager::ResetPickupState() to prevent a stale
// timer from a previous attempt from firing into a new pickup.
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::CancelPickupTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PickupPointTimerHandle);
    }
}

// ---------------------------------------------------------------------------
// FirePickupPoint � broadcasts OnPickupPoint at the visual moment the hand
// reaches the item. Called by AnimNotify_PickupPoint or the fallback timer.
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::FirePickupPoint()
{
    // Cancel the fallback timer in case the Notify fired first
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PickupPointTimerHandle);
    }

    UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] FirePickupPoint: listeners=%d"),
        OnPickupPoint.IsBound() ? 1 : 0);

    OnPickupPoint.Broadcast();
}



// ---------------------------------------------------------------------------
// HasCastReleaseSection
//   Returns true when the montage exposes a section named "CastRelease".
//   Used to decide between phase-based playback and the classic single-rate path.
// ---------------------------------------------------------------------------
