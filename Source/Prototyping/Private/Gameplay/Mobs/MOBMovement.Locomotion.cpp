// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include <Kismet/GameplayStatics.h>
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"

void UMOBMovementComponent::ProcessMovement(float DeltaTime)
{
    if (!GetOwner()) return;

    // Dispatch: patrol uses waypoint dead-reckoning with server correction,
    // combat states use server-position interpolation with extrapolation.
    if (CombatState == 0)
    {
        ProcessPatrolMovement(DeltaTime);
    }
    else
    {
        ProcessCombatMovement(DeltaTime);
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::HandleRotation(const FVector& MoveDir, float DeltaTime)
{
    if (MoveDir.IsNearlyZero()) return;

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;

    FRotator DesiredRot = MoveDir.Rotation();
    DesiredRot.Pitch = 0;
    DesiredRot.Roll = 0;

    const FRotator CurrentRot = Character->GetActorRotation();
    const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, DesiredRot.Yaw));

    // Speed up for large turns, keep normal speed for everything else.
    // Do NOT slow down for small angles — that prevented the mob from
    // ever finishing the turn to the correct heading.
    const float AdaptiveSpeed = (AngleDelta > 90.f)
        ? MoveRotationSpeed * 2.f
        : MoveRotationSpeed;

    Character->SetActorRotation(
        FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, AdaptiveSpeed));
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::UpdateMovingState(bool bNewIsMoving)
{
    if (bIsMoving != bNewIsMoving)
    {
        bIsMoving = bNewIsMoving;
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::SnapToGround()
{
    if (!GetOwner()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;

    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (!Capsule) return;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Character);
    Params.bTraceComplex = false;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

    const FVector Loc = GetOwner()->GetActorLocation();
    const FVector Start(Loc.X, Loc.Y, 100000.f);
    const FVector End(Loc.X, Loc.Y, -100000.f);
    FHitResult Hit;

    if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, Params))
    {
        const float DesiredZ = Hit.Location.Z + Capsule->GetScaledCapsuleHalfHeight();
        GetOwner()->SetActorLocation(FVector(Loc.X, Loc.Y, DesiredZ));
    }
}

// ---------------------------------------------------------------------------
