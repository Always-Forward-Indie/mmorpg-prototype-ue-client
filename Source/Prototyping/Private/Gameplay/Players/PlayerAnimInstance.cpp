// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

UPlayerAnimInstance::UPlayerAnimInstance()
{
}

void UPlayerAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    OwnerPlayer = Cast<ABasicPlayer>(TryGetPawnOwner());
    MontageEndedDelegate.BindUObject(this, &UPlayerAnimInstance::OnAttackMontageEnded);
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // Re-validate every tick: owner can change on respawn
    if (!OwnerPlayer.IsValid())
    {
        OwnerPlayer = Cast<ABasicPlayer>(TryGetPawnOwner());
    }

    if (!OwnerPlayer.IsValid()) return;

    ABasicPlayer* Player = OwnerPlayer.Get();

    // For remote (other-client) players the CharacterMovementComponent velocity is always
    // zero because movement is driven by SetActorLocation() not CMC.
    // Use RemoteSpeed, which is derived from consecutive server position deltas.
    // For the local player CMC velocity is authoritative � use it directly.
    if (Player->GetIsOtherClient())
    {
        Speed     = Player->GetRemoteSpeed();      // already EMA-smoothed in BasicPlayer
        bIsMoving = Speed > 1.0f;

        // Mirror CMC MaxWalkSpeed so the blend space normalisation works the same
        // for remote players as for the local player.
        const UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();
        MaxSpeed = (MoveComp && MoveComp->MaxWalkSpeed > 0.0f) ? MoveComp->MaxWalkSpeed : MaxSpeed;

        // Direction is EMA-smoothed in BasicPlayer::UpdateRemotePlayerMovement.
        // We read it unconditionally � it keeps its last value during the speed
        // fade-out so the blend-space doesn't snap to 0 while slowing down.
        Direction = Player->GetRemoteDirection();
    }
    else
    {
        const UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();
        Speed     = MoveComp ? MoveComp->Velocity.Size2D() : 0.0f;
        bIsMoving = Speed > 1.0f;
        MaxSpeed  = (MoveComp && MoveComp->MaxWalkSpeed > 0.0f) ? MoveComp->MaxWalkSpeed : MaxSpeed;

        // Compute direction angle between actor forward and velocity vector.
        // 0 = forward, -90 = left, +90 = right, �180 = backward.
        if (bIsMoving && MoveComp)
        {
            const FVector VelDir      = MoveComp->Velocity.GetSafeNormal2D();
            const FVector ActorForward = Player->GetActorForwardVector();
            const FVector ActorRight   = Player->GetActorRightVector();
            Direction = FMath::RadiansToDegrees(
                FMath::Atan2(FVector::DotProduct(VelDir, ActorRight),
                             FVector::DotProduct(VelDir, ActorForward)));
        }
        else
        {
            Direction = 0.0f;
        }
    }

    // SpeedNormalized: 0.0 = standing still, 1.0 = moving at full MaxSpeed.
    // Wire this into the Blend Space horizontal axis so animation playback
    // stays in sync regardless of the server-assigned move_speed value.
    SpeedNormalized = (MaxSpeed > 0.0f) ? FMath::Clamp(Speed / MaxSpeed, 0.0f, 1.0f) : 0.0f;

    // Fallback sync � event handlers are the primary source for these
    bIsDead = Player->GetIsDead();
}

// ---------------------------------------------------------------------------
// StartAttack
//   Called from BasicPlayer::PlaySkillAnimation_Implementation when the server
//   sends combatInitiation and this player is the caster.
//   animationDuration = how many seconds the server expects the full attack to take.
//   We compute PlayRate = montage_length / animationDuration so the clip finishes
//   exactly when the server considers the hit resolved (combatResult arrives).
//   A hit-point timer fires at DefaultHitRatio * animationDuration so that FCT
//   is shown at the visual moment of impact rather than at the end of the clip.
// ---------------------------------------------------------------------------
void UPlayerAnimInstance::StartAttack(const FSkillInitiationData& SkillData)
{
    if (bIsDead) return;

    // Reject re-entrant call: if a montage is already playing, don't restart it.
    // The upstream bIsCasting lock should prevent this, but guard here as a
    // safety net (e.g. broadcast packets arriving slightly out of order).
    if (bIsAttacking)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PlayerAnim] StartAttack ignored � already attacking (slot='%s')"),
            *CurrentAttackSlot.ToString());
        return;
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

    UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] StartAttack: slot='%s' duration=%.3fs montage=%s"),
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

            UE_LOG(LogTemp, Warning,
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

            UE_LOG(LogTemp, Warning,
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

        UE_LOG(LogTemp, Warning,
            TEXT("[PlayerAnim] No montage for slot '%s'. Maps: %d entries, DefaultAttackMontage=%s"),
            *CurrentAttackSlot.ToString(), AttackMontageMap.Num(),
            DefaultAttackMontage ? *DefaultAttackMontage->GetName() : TEXT("nullptr"));
        UE_LOG(LogTemp, Warning,
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

    UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] FireHitPoint: casterId=%d listeners=%d"),
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
        UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] OnCastReleaseTimer: active montage is gone, skipping"));
        return;
    }

    // The montage should have naturally transitioned Default→CastRelease by now
    // (AutoRate was calculated to finish the Default section at exactly T=castTime).
    // Only force-jump if float imprecision left us still in Default.
    const FName CurrentSection = Montage_GetCurrentSection(Montage);
    if (CurrentSection != FName("CastRelease"))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[PlayerAnim] OnCastReleaseTimer: still in section '%s', forcing JumpToSection"),
            *CurrentSection.ToString());
        Montage_JumpToSection(FName("CastRelease"), Montage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] OnCastReleaseTimer: in CastRelease, setting rate=1.0"));
    }

    // Ensure CastRelease always plays at the animator's intended 1.0x speed.
    Montage_SetPlayRate(Montage, 1.0f);
    CurrentAttackPlayRate = 1.0f;
}
// ---------------------------------------------------------------------------
// NotifyDeath
//   Latch bIsDead, stop any active montage, let the State Machine take over.
// ---------------------------------------------------------------------------
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
        UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] NotifyPickup: PickupMontage is NULL - skipping montage"));
        return 0.0f;
    }

    const float PlayRate     = 1.0f;
    const float MontageLen   = PickupMontage->GetPlayLength();
    const float Duration     = MontageLen / PlayRate;
    const float PickupDelay  = Duration * 0.5f;

    UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] NotifyPickup: playing montage '%s' len=%.3fs delay=%.3fs"),
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

    UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] FirePickupPoint: listeners=%d"),
        OnPickupPoint.IsBound() ? 1 : 0);

    OnPickupPoint.Broadcast();
}



// ---------------------------------------------------------------------------
// HasCastReleaseSection
//   Returns true when the montage exposes a section named "CastRelease".
//   Used to decide between phase-based playback and the classic single-rate path.
// ---------------------------------------------------------------------------
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
                UE_LOG(LogTemp, Log,
                    TEXT("[PlayerAnim] HitDelay from notify '%s': montageTime=%.3fs playRate=%.3f wallClock=%.3fs"),
                    *NotifyName.ToString(), NotifyEvent.GetTriggerTime(), PlayRate, WallClockTime);
                return WallClockTime;
            }
        }
        UE_LOG(LogTemp, Log,
            TEXT("[PlayerAnim] No HitPoint notify found in montage '%s', using DefaultHitRatio fallback"),
            *Montage->GetName());
    }
    return ActualDuration * DefaultHitRatio;
}

void UPlayerAnimInstance::OnAttackMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
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

    // Notify listeners (e.g. PlayerSkillManager) that the animation is done
    OnAttackEnded.Broadcast();
}
