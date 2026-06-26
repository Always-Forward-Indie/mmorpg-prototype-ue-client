// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBAnimInstance.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Animation/AnimMontage.h"

UMOBAnimInstance::UMOBAnimInstance()
{
}

void UMOBAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerMOB = Cast<ABasicMOB>(TryGetPawnOwner());
	MontageEndedDelegate.BindUObject(this, &UMOBAnimInstance::OnAttackMontageEnded);
}

void UMOBAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerMOB.IsValid()) return;

	ABasicMOB* MOB = OwnerMOB.Get();

	// Use the server-driven movement component speed so bIsMoving and Speed
	// reflect network-authoritative state rather than CharacterMovementComponent
	// (which is always 0 because we use SetActorLocation, not AddMovementInput).
	if (UMOBMovementComponent* MoveComp = MOB->MOBMovementComponent)
	{
		Speed      = MoveComp->GetCurrentSpeed();
		bIsMoving  = MoveComp->IsMoving();
		bIsFleeing = MoveComp->IsFleeing();
	}
	else
	{
		Speed      = 0.0f;
		bIsMoving  = MOB->GetMOBIsMoving();
		bIsFleeing = false;
	}

	// Fallback sync � event handlers are the primary source for these
	bIsAggressive = MOB->GetMOBIsAggressive();
	bIsDead       = MOB->GetMOBIsDead();
}

// ---------------------------------------------------------------------------
// StartAttack
//   Called from BasicMOB::OnReceiveSkillInitiation when this mob is the caster.
//   animationDuration = how many seconds the server expects the full attack to take.
//   We compute PlayRate = montage_length / animationDuration so the clip finishes
//   exactly when the server considers the hit resolved (combatResult arrives).
//   A hit-point timer fires at DefaultHitRatio * animationDuration so that FCT
//   is shown at the visual moment of impact rather than at the end of the clip.
// ---------------------------------------------------------------------------
void UMOBAnimInstance::StartAttack(const FSkillInitiationData& SkillData)
{
	if (bIsDead) return;

	// Interrupt any playing hit-react montage so it doesn't overlap with the attack
	if (HitReactMontage && bIsHit)
	{
		bIsHit = false;
		Montage_Stop(0.0f, HitReactMontage);
	}

	bIsAttacking      = true;
	CurrentAttackSlot = FName(*SkillData.animationName);
	CurrentCasterId   = SkillData.casterId;

	// Cancel any previous hit-point timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitPointTimerHandle);
	}

	UAnimMontage* Montage = ResolveMontage(CurrentAttackSlot);

	if (Montage)
	{
		const float PlayRate = CalcPlayRate(Montage, SkillData.animationDuration);
		CurrentAttackPlayRate = PlayRate;

		Montage_Play(Montage, PlayRate);
		Montage_SetEndDelegate(MontageEndedDelegate, Montage);

		// Actual wall-clock duration after PlayRate clamping:
		//   e.g. montage=1.5s, server wants 0.3s -> PlayRate clamped to 3.0
		//   -> actual playback = 1.5/3.0 = 0.5s, not 0.3s.
		// HitDelay must track the real playback time, not the server-desired duration.
		const float MontageLength  = Montage->GetPlayLength();
		const float ActualDuration = (PlayRate > 0.0f) ? MontageLength / PlayRate : SkillData.animationDuration;
		const float HitDelay       = CalcHitDelay(Montage, PlayRate, ActualDuration);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(HitPointTimerHandle, this,
				&UMOBAnimInstance::FireHitPoint, HitDelay, false);
		}
	}
	else
	{
		// No montage assigned yet � fall back to a timer so bIsAttacking is still cleared
		CurrentAttackPlayRate = 1.0f;
		const float Duration = FMath::Max(SkillData.animationDuration, 0.1f);

		if (UWorld* World = GetWorld())
		{
			// Hit-point fallback
			const float HitDelay = Duration * DefaultHitRatio;
			World->GetTimerManager().SetTimer(HitPointTimerHandle, this,
				&UMOBAnimInstance::FireHitPoint, HitDelay, false);

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
void UMOBAnimInstance::FireHitPoint()
{
	// Cancel the fallback timer � in case the Notify fired first
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitPointTimerHandle);
	}

	if (CurrentCasterId > 0)
	{
		OnHitPoint.Broadcast(CurrentCasterId);
	}
}

// ---------------------------------------------------------------------------
// NotifyDeath
//   Latch bIsDead, stop any active montage, play death montage if assigned.
// ---------------------------------------------------------------------------
void UMOBAnimInstance::NotifyDeath()
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
// NotifyTargetLost  (mobTargetLost packet)
// ---------------------------------------------------------------------------
void UMOBAnimInstance::NotifyTargetLost()
{
	bIsAggressive     = false;
	bIsAttacking      = false;
	CurrentAttackSlot = NAME_None;

	// Blend out attack montage if one is playing
	Montage_StopGroupByName(0.2f, FName("DefaultSlot"));
}

// ---------------------------------------------------------------------------
// NotifyHit  (combatResult � this mob is the TARGET)
//   Plays a hit-react montage over the current locomotion/combat-idle.
//   Clears bIsHit automatically when the montage ends.
// ---------------------------------------------------------------------------
void UMOBAnimInstance::NotifyHit()
{
	if (bIsDead)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MOBAnim] NotifyHit skipped: mob is dead"));
		return;
	}

	// Hit-react takes priority — interrupt any active attack
	if (bIsAttacking)
	{
		bIsAttacking = false;
		CurrentAttackSlot = NAME_None;
		Montage_StopGroupByName(0.0f, FName("DefaultGroup"));
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HitPointTimerHandle);
		}
	}

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
		UE_LOG(LogTemp, Warning, TEXT("[MOBAnim] NotifyHit: HitReactMontage is null, using 0.3s timer fallback"));
		// No montage - clear after a fixed window
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
// Private helpers
// ---------------------------------------------------------------------------

UAnimMontage* UMOBAnimInstance::ResolveMontage(const FName& AnimationName) const
{
	if (const UAnimMontage* const* Found = AttackMontageMap.Find(AnimationName))
	{
		if (*Found) return const_cast<UAnimMontage*>(*Found);
	}
	return DefaultAttackMontage;
}

float UMOBAnimInstance::CalcPlayRate(const UAnimMontage* Montage, float DesiredDuration)
{
	if (!Montage || DesiredDuration <= 0.0f) return 1.0f;

	const float MontageLength = Montage->GetPlayLength();
	if (MontageLength <= 0.0f) return 1.0f;

	// Clamp to a sane range so we never play absurdly fast or slow
	return FMath::Clamp(MontageLength / DesiredDuration, 0.5f, 3.0f);
}

float UMOBAnimInstance::CalcHitDelay(const UAnimMontage* Montage, float PlayRate, float ActualDuration)
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
					TEXT("[MOBAnim] HitDelay from notify '%s': montageTime=%.3fs playRate=%.3f wallClock=%.3fs"),
					*NotifyName.ToString(), NotifyEvent.GetTriggerTime(), PlayRate, WallClockTime);
				return WallClockTime;
			}
		}
		UE_LOG(LogTemp, Log,
			TEXT("[MOBAnim] No HitPoint notify found in montage '%s', using DefaultHitRatio fallback"),
			*Montage->GetName());
	}
	return ActualDuration * DefaultHitRatio;
}

void UMOBAnimInstance::OnAttackMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
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

	// Ensure the hit-point timer is cleared if it somehow didn't fire
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitPointTimerHandle);
	}

	// If the montage was interrupted (e.g. by death or stun) and the hit-point
	// never fired, flush any pending results now so damage doesn't wait 12 seconds.
	if (bInterrupted && EndedCasterId > 0)
	{
		OnHitPoint.Broadcast(EndedCasterId);
	}
}
