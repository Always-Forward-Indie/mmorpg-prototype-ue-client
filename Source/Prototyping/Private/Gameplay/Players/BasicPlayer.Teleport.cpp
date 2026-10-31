#include "Gameplay/Players/BasicPlayer.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Utils/NetCore/TeleportRule.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

bool ABasicPlayer::SnapToAuthoritativePosition(const FVector& Target, float Yaw, FString& OutError)
{
	if (!FTeleportRule::IsValidTarget(Target.X, Target.Y, Target.Z, Yaw))
	{
		OutError = FString::Printf(TEXT("insane target (%.0f, %.0f, %.0f yaw=%.0f)"), Target.X, Target.Y, Target.Z, Yaw);
		return false;
	}

	const float NormYaw = FTeleportRule::NormalizeYaw(Yaw);

	// Drain motion state so no residual velocity/input leaks past the snap.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
		FVector Vel = CMC->Velocity;
		Vel.X = 0.0f;
		Vel.Y = 0.0f;
		CMC->Velocity = Vel;
		CMC->UpdateComponentVelocity();
	}
	ConsumeMovementInputVector();

	SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator(0.0f, NormYaw, 0.0f));
	if (AController* C = GetController())
	{
		C->SetControlRotation(FRotator(0.0f, NormYaw, 0.0f));
	}
	SetCoordinates(Target.X, Target.Y, Target.Z, NormYaw, false);
	UpdateOverlaps();

	return true;
}

void ABasicPlayer::CancelActionsForTeleport()
{
	StopAutoAttack();
	ClearLockedTarget();
	ClearTarget_Implementation();
	HideCastBar_Implementation();

	bIsCasting = false;
	bIsPickingUp = false;
	bIsApproachingTarget = false;
	PendingSkillSlug.Empty();
	PendingInteraction = EPendingInteraction::None;
	PendingInteractionTarget.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);
		World->GetTimerManager().ClearTimer(PickupSafetyTimerHandle);
	}

	// Queued damage/healing must not land after the jump (same rule as death).
	if (!playerData.isOtherClient && MyGameInstance)
	{
		if (UCombatSystemManager* CSM = MyGameInstance->GetCombatSystemManager())
		{
			CSM->FlushAllPendingForCaster(GetActorId_Implementation());
		}
	}
}

void ABasicPlayer::BeginTeleportCover(const FString& Reason)
{
	if (bTeleportCoverActive)
	{
		return;
	}
	bTeleportCoverActive = true;

	CancelActionsForTeleport();
	LockMovement();

	// Freeze the boom: with lag on, the camera would swing through the world
	// from the old position to the new one. Re-enabled on reveal.
	if (CameraBoom)
	{
		CameraBoom->bEnableCameraLag = false;
	}

	if (!playerData.isOtherClient)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC && GetWorld())
		{
			PC = GetWorld()->GetFirstPlayerController();
		}
		if (PC && PC->PlayerCameraManager)
		{
			// Hold black through the hold phase; the snap lands invisible.
			PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FTeleportRule::FadeOutSec, FLinearColor::Black, false, true);
		}
	}

	UE_LOG(LogGameplay, Verbose, TEXT("TeleportCover: begin (%s) at (%.0f, %.0f, %.0f)"),
		*Reason, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
}

void ABasicPlayer::FinishTeleportReveal()
{
	if (!bTeleportCoverActive)
	{
		return;
	}
	bTeleportCoverActive = false;

	if (CameraBoom)
	{
		CameraBoom->bEnableCameraLag = true;
	}

	if (!playerData.isOtherClient)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC && GetWorld())
		{
			PC = GetWorld()->GetFirstPlayerController();
		}
		if (PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FTeleportRule::FadeInSec, FLinearColor::Black);
		}
	}

	UnlockMovement();
}

bool ABasicPlayer::SnapToGround(float UpTraceCm, float DownTraceCm)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Center = GetActorLocation();
	const FVector From = Center + FVector(0.0f, 0.0f, UpTraceCm);
	const FVector To = Center - FVector(0.0f, 0.0f, DownTraceCm);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TeleportSnapToGround), false, this);
	if (World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Params) && Hit.bBlockingHit)
	{
		float HalfHeight = 90.0f;
		if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
		SetActorLocation(Hit.Location + FVector(0.0f, 0.0f, HalfHeight + 2.0f), false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}
	return false;
}
