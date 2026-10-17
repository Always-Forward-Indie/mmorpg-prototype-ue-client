// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/Ambient/AmbientCreatureActor.h"
#include "Gameplay/Ambient/AmbientCreatureDefinition.h"
#include "Gameplay/Ambient/AmbientScheduleAsset.h"
#include "Gameplay/Ambient/AmbientBehaviorBase.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "DrawDebugHelpers.h"

void AAmbientCreatureActor::StartIdleAnimCycle()
{
	if (!CreatureDefinition) return;

	bIdleCycleActive = true;

	// Play the default idle montage immediately if assigned
	if (CreatureDefinition->DefaultIdleMontage && !bIdleVariantPlaying)
	{
		const float Duration = PlayAnimMontage(CreatureDefinition->DefaultIdleMontage);
		if (Duration > 0.f)
		{
			ActiveIdleMontage = CreatureDefinition->DefaultIdleMontage;
			bIdleVariantPlaying = true;

			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				IdleEndedDelegate.BindUObject(this, &AAmbientCreatureActor::OnIdleVariantMontageEnded);
				AnimInst->Montage_SetEndDelegate(IdleEndedDelegate, CreatureDefinition->DefaultIdleMontage);
			}
		}
	}

	ScheduleNextIdleVariant();
}

void AAmbientCreatureActor::ScheduleNextIdleVariant()
{
	if (!GetWorld() || !CreatureDefinition) return;
	if (!bIdleCycleActive) return;

	const float Delay = FMath::FRandRange(
		CreatureDefinition->IdleVariantMinDelay,
		CreatureDefinition->IdleVariantMaxDelay);

	GetWorld()->GetTimerManager().SetTimer(
		IdleVariantTimerHandle,
		this,
		&AAmbientCreatureActor::TriggerRandomIdleVariant,
		Delay,
		false);
}

void AAmbientCreatureActor::TriggerRandomIdleVariant()
{
	if (!bIdleCycleActive || bIdleVariantPlaying) return;
	if (!CreatureDefinition || CreatureDefinition->IdleVariantMontages.Num() == 0)
	{
		ScheduleNextIdleVariant();
		return;
	}

	const int32 Idx = FMath::RandRange(0, CreatureDefinition->IdleVariantMontages.Num() - 1);
	UAnimMontage* Montage = CreatureDefinition->IdleVariantMontages[Idx];
	if (!Montage) { ScheduleNextIdleVariant(); return; }

	const float Duration = PlayAnimMontage(Montage);
	if (Duration > 0.f)
	{
		ActiveIdleMontage = Montage;
		bIdleVariantPlaying = true;

		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			IdleEndedDelegate.BindUObject(this, &AAmbientCreatureActor::OnIdleVariantMontageEnded);
			AnimInst->Montage_SetEndDelegate(IdleEndedDelegate, Montage);
		}
	}
	else
	{
		ScheduleNextIdleVariant();
	}
}

void AAmbientCreatureActor::OnIdleVariantMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (ActiveIdleMontage != Montage) return;
	ActiveIdleMontage = nullptr;
	bIdleVariantPlaying = false;

	if (!bInterrupted && bIdleCycleActive)
	{
		// Return to default idle if assigned, then schedule next variant
		if (CreatureDefinition && CreatureDefinition->DefaultIdleMontage)
		{
			PlayAnimMontage(CreatureDefinition->DefaultIdleMontage);
		}
		ScheduleNextIdleVariant();
	}
}

void AAmbientCreatureActor::InterruptIdleCycle()
{
	bIdleCycleActive = false;
	GetWorld()->GetTimerManager().ClearTimer(IdleVariantTimerHandle);

	if (bIdleVariantPlaying && ActiveIdleMontage)
	{
		StopAnimMontage(ActiveIdleMontage);
		ActiveIdleMontage = nullptr;
		bIdleVariantPlaying = false;
	}
}

void AAmbientCreatureActor::ResumeIdleCycle()
{
	bIdleCycleActive = true;
	StartIdleAnimCycle();
}

// ─────────────────────────────────────────────────────────────────────────────
// Idle Sound Cycle
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::ScheduleNextIdleSound()
{
	if (!GetWorld() || !CreatureDefinition || CreatureDefinition->IdleSounds.Num() == 0) return;

	const float Delay = FMath::FRandRange(
		CreatureDefinition->IdleSoundMinDelay,
		CreatureDefinition->IdleSoundMaxDelay);

	GetWorld()->GetTimerManager().SetTimer(
		IdleSoundTimerHandle,
		this,
		&AAmbientCreatureActor::PlayRandomIdleSound,
		Delay,
		false);
}

void AAmbientCreatureActor::PlayRandomIdleSound()
{
	if (!CreatureDefinition || CreatureDefinition->IdleSounds.Num() == 0) return;

	const int32 Idx = FMath::RandRange(0, CreatureDefinition->IdleSounds.Num() - 1);
	USoundBase* Sound = CreatureDefinition->IdleSounds[Idx];

	if (Sound && AudioComponent)
	{
		AudioComponent->SetSound(Sound);
		AudioComponent->Play();
	}

	ScheduleNextIdleSound();
}

// ─────────────────────────────────────────────────────────────────────────────
// LOD — reduce tick rate based on distance to player
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::UpdateLOD()
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->GetPawn()) return;

	const float Dist = FVector::Dist(GetActorLocation(), PC->GetPawn()->GetActorLocation());

	if (Dist > 10000.f)
	{
		SetActorTickEnabled(false);
	}
	else if (Dist > 5000.f)
	{
		SetActorTickEnabled(true);
		SetActorTickInterval(0.5f);
	}
	else
	{
		SetActorTickEnabled(true);
		SetActorTickInterval(0.1f);
	}
}
