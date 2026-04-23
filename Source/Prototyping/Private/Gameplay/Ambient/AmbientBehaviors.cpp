// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/Ambient/AmbientBehaviors.h"
#include "Gameplay/Ambient/AmbientCreatureActor.h"
#include "Components/SplineComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_Wait
// ─────────────────────────────────────────────────────────────────────────────

void UAmbientBehavior_Wait::Execute_Implementation(AAmbientCreatureActor* Owner)
{
	if (!Owner) return;
	const float Duration = FMath::FRandRange(MinDuration, MaxDuration);
	if (const UWorld* World = Owner->GetWorld())
	{
		EndTime = World->GetTimeSeconds() + Duration;
	}
	bStarted = true;
}

bool UAmbientBehavior_Wait::IsComplete_Implementation(AAmbientCreatureActor* Owner) const
{
	if (!bStarted || !Owner) return true;
	const UWorld* World = Owner->GetWorld();
	return World && World->GetTimeSeconds() >= EndTime;
}

void UAmbientBehavior_Wait::OnAbort_Implementation(AAmbientCreatureActor* Owner)
{
	bStarted = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_MoveToSplinePoint
// ─────────────────────────────────────────────────────────────────────────────

void UAmbientBehavior_MoveToSplinePoint::Execute_Implementation(AAmbientCreatureActor* Owner)
{
	if (!Owner) return;
	bArrived = false;

	USplineComponent* Spline = Owner->GetPathSpline();
	if (!Spline)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAmbientBehavior_MoveToSplinePoint: Owner '%s' has no PathSpline."), *Owner->GetName());
		bArrived = true;
		return;
	}

	const int32 NumPoints = Spline->GetNumberOfSplinePoints();
	if (NumPoints == 0 || PointIndex >= NumPoints)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAmbientBehavior_MoveToSplinePoint: PointIndex %d out of range (%d points)."), PointIndex, NumPoints);
		bArrived = true;
		return;
	}

	const FVector TargetLocation = Spline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
	Owner->MoveToLocation(TargetLocation, AcceptanceRadius, FSimpleDelegate::CreateWeakLambda(Owner, [this]()
	{
		bArrived = true;
	}));
}

bool UAmbientBehavior_MoveToSplinePoint::IsComplete_Implementation(AAmbientCreatureActor* Owner) const
{
	return bArrived;
}

void UAmbientBehavior_MoveToSplinePoint::OnAbort_Implementation(AAmbientCreatureActor* Owner)
{
	if (Owner) Owner->StopMovement();
	bArrived = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_MoveToRandomPoint
// ─────────────────────────────────────────────────────────────────────────────

void UAmbientBehavior_MoveToRandomPoint::Execute_Implementation(AAmbientCreatureActor* Owner)
{
	if (!Owner) return;
	bArrived = false;

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Owner->GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAmbientBehavior_MoveToRandomPoint: No NavigationSystem found."));
		bArrived = true;
		return;
	}

	FNavLocation NavLoc;
	ANavigationData* NavData = NavSys->GetNavDataForProps(Owner->GetNavAgentProps(), Owner->GetActorLocation());
	const bool bFound = NavSys->GetRandomReachablePointInRadius(Owner->GetHomeLocation(), SearchRadius, NavLoc, NavData);
	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAmbientBehavior_MoveToRandomPoint: No reachable point found in radius %.0f."), SearchRadius);
		bArrived = true;
		return;
	}

	Owner->MoveToLocation(NavLoc.Location, AcceptanceRadius, FSimpleDelegate::CreateWeakLambda(Owner, [this]()
	{
		bArrived = true;
	}));
}

bool UAmbientBehavior_MoveToRandomPoint::IsComplete_Implementation(AAmbientCreatureActor* Owner) const
{
	return bArrived;
}

void UAmbientBehavior_MoveToRandomPoint::OnAbort_Implementation(AAmbientCreatureActor* Owner)
{
	if (Owner) Owner->StopMovement();
	bArrived = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_PlayMontage
// ─────────────────────────────────────────────────────────────────────────────

void UAmbientBehavior_PlayMontage::Execute_Implementation(AAmbientCreatureActor* Owner)
{
	if (!Owner || !Montage) return;
	bMontageEnded = false;

	// Interrupt any running idle variant to give this montage full control
	Owner->InterruptIdleCycle();

	const float Duration = Owner->PlayAnimMontage(Montage);
	if (Duration <= 0.f)
	{
		bMontageEnded = true;
		return;
	}

	if (bWaitForEnd)
	{
		// Bind end delegate on the anim instance
		if (USkeletalMeshComponent* Mesh = Owner->GetMesh())
		{
			if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
			{
				FOnMontageEnded EndDelegate;
				EndDelegate.BindWeakLambda(Owner, [this](UAnimMontage* /*M*/, bool /*bInterrupted*/)
				{
					bMontageEnded = true;
				});
				AnimInst->Montage_SetEndDelegate(EndDelegate, Montage);
			}
		}
	}
	else
	{
		// Use timer-based duration instead
		if (const UWorld* World = Owner->GetWorld())
		{
			EndTime = World->GetTimeSeconds() + FMath::FRandRange(MinDuration, MaxDuration);
		}
	}
}

bool UAmbientBehavior_PlayMontage::IsComplete_Implementation(AAmbientCreatureActor* Owner) const
{
	if (!Owner) return true;

	if (bWaitForEnd)
	{
		return bMontageEnded;
	}

	const UWorld* World = Owner->GetWorld();
	return World && World->GetTimeSeconds() >= EndTime;
}

void UAmbientBehavior_PlayMontage::OnAbort_Implementation(AAmbientCreatureActor* Owner)
{
	if (Owner && Montage)
	{
		Owner->StopAnimMontage(Montage);
	}
	bMontageEnded = true;
	// Resume idle cycle
	if (Owner) Owner->ResumeIdleCycle();
}

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_WanderSteps
// ─────────────────────────────────────────────────────────────────────────────

void UAmbientBehavior_WanderSteps::Execute_Implementation(AAmbientCreatureActor* Owner)
{
	if (!Owner) return;
	bAllDone = false;
	StepsRemaining = FMath::Max(1, StepCount);
	StartNextStep(Owner);
}

void UAmbientBehavior_WanderSteps::StartNextStep(AAmbientCreatureActor* Owner)
{
	if (!Owner || StepsRemaining <= 0)
	{
		bAllDone = true;
		return;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Owner->GetWorld());
	if (!NavSys)
	{
		bAllDone = true;
		return;
	}

	FNavLocation NavLoc;
	ANavigationData* NavData = NavSys->GetNavDataForProps(Owner->GetNavAgentProps(), Owner->GetActorLocation());
	const bool bFound = NavSys->GetRandomReachablePointInRadius(Owner->GetHomeLocation(), Radius, NavLoc, NavData);
	if (!bFound)
	{
		--StepsRemaining;
		if (StepsRemaining <= 0) { bAllDone = true; return; }
		// Try again after a short pause instead of recursing synchronously
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				StepPauseTimerHandle,
				FTimerDelegate::CreateWeakLambda(Owner, [this, Owner]() { StartNextStep(Owner); }),
				0.5f, false);
		}
		return;
	}

	Owner->MoveToLocation(NavLoc.Location, AcceptanceRadius, FSimpleDelegate::CreateWeakLambda(Owner, [this, Owner]()
	{
		--StepsRemaining;
		if (StepsRemaining <= 0)
		{
			bAllDone = true;
			return;
		}
		// Schedule pause, then next step via timer — never recurse synchronously
		if (UWorld* World = Owner->GetWorld())
		{
			const float Pause = FMath::FRandRange(PauseMin, PauseMax);
			World->GetTimerManager().SetTimer(
				StepPauseTimerHandle,
				FTimerDelegate::CreateWeakLambda(Owner, [this, Owner]() { StartNextStep(Owner); }),
				FMath::Max(Pause, 0.05f), false);
		}
	}));
}

bool UAmbientBehavior_WanderSteps::IsComplete_Implementation(AAmbientCreatureActor* Owner) const
{
	return bAllDone;
}

void UAmbientBehavior_WanderSteps::OnAbort_Implementation(AAmbientCreatureActor* Owner)
{
	if (Owner)
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetTimerManager().ClearTimer(StepPauseTimerHandle);
		}
		Owner->StopMovement();
	}
	bAllDone = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_FlyAlong
// ─────────────────────────────────────────────────────────────────────────────

void UAmbientBehavior_FlyAlong::Execute_Implementation(AAmbientCreatureActor* Owner)
{
	if (!Owner) return;
	bFinished = false;
	CurrentDistance = 0.f;

	// Disable gravity for aerial movement
	if (UCharacterMovementComponent* CMC = Owner->GetCharacterMovement())
	{
		CMC->GravityScale = 0.f;
		CMC->SetMovementMode(MOVE_Flying);
	}

	if (StartDelay > 0.f)
	{
		bDelaying = true;
		if (const UWorld* World = Owner->GetWorld())
		{
			DelayEndTime = World->GetTimeSeconds() + StartDelay;
		}
	}
	else
	{
		bDelaying = false;
	}
}

bool UAmbientBehavior_FlyAlong::IsComplete_Implementation(AAmbientCreatureActor* Owner) const
{
	if (!Owner) return true;
	if (bFinished) return true;

	const UWorld* World = Owner->GetWorld();
	if (!World) return true;

	// Still in start delay
	if (bDelaying)
	{
		if (World->GetTimeSeconds() < DelayEndTime) return false;
		const_cast<UAmbientBehavior_FlyAlong*>(this)->bDelaying = false;
	}

	USplineComponent* Spline = Owner->GetPathSpline();
	if (!Spline)
	{
		const_cast<UAmbientBehavior_FlyAlong*>(this)->bFinished = true;
		return true;
	}

	const float SplineLength = Spline->GetSplineLength();
	if (SplineLength <= 0.f)
	{
		const_cast<UAmbientBehavior_FlyAlong*>(this)->bFinished = true;
		return true;
	}

	// Advance along spline
	const float DeltaTime = World->GetDeltaSeconds();
	float& Dist = const_cast<UAmbientBehavior_FlyAlong*>(this)->CurrentDistance;
	Dist += FlySpeed * DeltaTime;

	if (Dist >= SplineLength)
	{
		if (bLoopFlight)
		{
			Dist = 0.f; // Teleport back silently — schedule will loop entry
		}
		else
		{
			Dist = SplineLength;
			const_cast<UAmbientBehavior_FlyAlong*>(this)->bFinished = true;
		}
	}

	// Move actor along spline
	const FVector NewLocation = Spline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
	const FRotator NewRotation = Spline->GetRotationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
	const_cast<AAmbientCreatureActor*>(Owner)->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

	return bLoopFlight ? false : bFinished;
}

void UAmbientBehavior_FlyAlong::OnAbort_Implementation(AAmbientCreatureActor* Owner)
{
	// Restore gravity
	if (Owner)
	{
		if (UCharacterMovementComponent* CMC = Owner->GetCharacterMovement())
		{
			CMC->GravityScale = 1.f;
			CMC->SetMovementMode(MOVE_Walking);
		}
	}
	bFinished = true;
}
