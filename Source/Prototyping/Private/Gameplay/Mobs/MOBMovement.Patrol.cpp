// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include <Kismet/GameplayStatics.h>
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"

void UMOBMovementComponent::ProcessPatrolMovement(float DeltaTime)
{
    if (!GetOwner()) return;

    const FVector CurrentLocation = GetOwner()->GetActorLocation();

    // --- Active blend from state transition (e.g. combat -> patrol) -----------
    if (bBlendActive)
    {
        BlendElapsed += DeltaTime;
        const float Alpha = FMath::Clamp(BlendElapsed / FMath::Max(BlendDuration, 0.001f), 0.f, 1.f);
        // Use BlendToPos (captured at blend-start), NOT LatestServerPos.
        // New patrol packets must not change the blend target mid-animation;
        // ProcessPatrolMovement takes over from LatestServerPos after the blend.
        FVector BlendedPos = FMath::Lerp(
            FVector(BlendFromPos.X, BlendFromPos.Y, CurrentLocation.Z),
            FVector(BlendToPos.X, BlendToPos.Y, CurrentLocation.Z),
            Alpha);
        BlendedPos = AdjustToGround(BlendedPos, DeltaTime, 1.0f);
        GetOwner()->SetActorLocation(BlendedPos);

        // Rotate toward actual movement direction during the blend.
        const FVector BlendMoveDelta(BlendedPos.X - PrevFramePos.X,
                                     BlendedPos.Y - PrevFramePos.Y, 0.f);
        const float BlendMoveDist = BlendMoveDelta.Size();
        if (BlendMoveDist > 0.5f)
        {
            const FVector BlendMoveDir = BlendMoveDelta / BlendMoveDist;
            SmoothedFacingDir = FMath::VInterpNormalRotationTo(
                SmoothedFacingDir, BlendMoveDir, DeltaTime, PatrolRotationRate);

            ACharacter* BlendChar = Cast<ACharacter>(GetOwner());
            if (BlendChar)
            {
                FRotator DesiredRot = SmoothedFacingDir.Rotation();
                DesiredRot.Pitch = 0.f;
                DesiredRot.Roll  = 0.f;
                BlendChar->SetActorRotation(
                    FMath::RInterpConstantTo(
                        BlendChar->GetActorRotation(), DesiredRot,
                        DeltaTime, PatrolRotationRate));
            }
        }

        PrevFramePos = BlendedPos;

        if (Alpha >= 1.f)
        {
            bBlendActive = false;
        }

        CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed, PatrolSpeed, DeltaTime, 5.f);
        UpdateMovingState(CurrentInterpSpeed > MovingStartThreshold);
        return;
    }

    // Server target in client space (keep Z from actor — server sends flat Z)
    const FVector TargetXY(LatestServerPos.X, LatestServerPos.Y, CurrentLocation.Z);
    const float GapToServer = FVector::Dist2D(CurrentLocation, TargetXY);

    // --- Stop condition -------------------------------------------------------
    // 5-unit hysteresis: once inside, stay until next packet resets state.
    // bHasWaypoint is intentionally NOT checked here: the mob should move
    // whenever the server reports speed > 0 and a position we haven't reached
    // yet.  Requiring a waypoint blocked zone-1 (ANNULUS) mobs whose server
    // packets legitimately omit the waypoint field.
    if (PatrolSpeed < 1.f || GapToServer < 5.f)
    {
        // Only settle toward server pos for small corrections (< PatrolSnapThreshold).
        // Larger gaps mean LatestServerPos jumped far (e.g. first packet after spawn
        // or waypoint-reached packet) — sliding hundreds of units without walk
        // animation looks like a teleport.  Stay put and wait for a proper
        // movement packet instead.
        FVector SettleLoc;
        if (GapToServer > 2.f && GapToServer <= PatrolSnapThreshold)
        {
            SettleLoc = FMath::VInterpTo(CurrentLocation, TargetXY, DeltaTime, 8.f);
        }
        else
        {
            SettleLoc = CurrentLocation;
        }
        SettleLoc = AdjustToGround(SettleLoc, DeltaTime, 1.0f);
        GetOwner()->SetActorLocation(SettleLoc);
        PrevFramePos = SettleLoc;
        // Mob has reached its destination — kill speed and animation immediately.
        // Lerping here only delays the idle transition visually.
        CurrentInterpSpeed = 0.f;
        UpdateMovingState(false);
        return;
    }

    // --- Direction to server-authoritative position --------------------------
    const FVector DirToServer = (TargetXY - CurrentLocation).GetSafeNormal2D();

    // --- Rotate-in-place before walking --------------------------------------
    // When starting from idle and facing is far from the target direction,
    // turn first so the mob doesn't visually slide sideways.
    if (CurrentInterpSpeed < MovingStartThreshold && !DirToServer.IsNearlyZero())
    {
        const float DotToTarget = FVector::DotProduct(
            SmoothedFacingDir.GetSafeNormal2D(), DirToServer);
        const float AngleDeg = FMath::RadiansToDegrees(
            FMath::Acos(FMath::Clamp(DotToTarget, -1.f, 1.f)));

        if (AngleDeg > PatrolTurnStartAngle)
        {
            // Rotate SmoothedFacingDir toward target at a controlled rate.
            SmoothedFacingDir = FMath::VInterpNormalRotationTo(
                SmoothedFacingDir, DirToServer, DeltaTime, PatrolRotationRate);

            ACharacter* Character = Cast<ACharacter>(GetOwner());
            if (Character)
            {
                FRotator DesiredRot = SmoothedFacingDir.Rotation();
                DesiredRot.Pitch = 0.f;
                DesiredRot.Roll  = 0.f;
                Character->SetActorRotation(
                    FMath::RInterpConstantTo(
                        Character->GetActorRotation(), DesiredRot,
                        DeltaTime, PatrolRotationRate));
            }

            FVector GroundLoc = AdjustToGround(CurrentLocation, DeltaTime, 1.0f);
            GetOwner()->SetActorLocation(GroundLoc);
            PrevFramePos = GroundLoc;
            UpdateMovingState(false);
            return;
        }
    }

    // --- Move straight toward server-authoritative position ------------------
    CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed, PatrolSpeed, DeltaTime, 5.f);
    const float StepDist = FMath::Min(CurrentInterpSpeed * DeltaTime, GapToServer - 4.f);
    FVector NewLocation = CurrentLocation + DirToServer * FMath::Max(StepDist, 0.f);

    NewLocation = AdjustToGround(NewLocation, DeltaTime, 1.0f);
    GetOwner()->SetActorLocation(NewLocation);

    // Rotation: smooth constant-rate turn driven by actual movement delta.
    // Uses PatrolRotationRate instead of the combat HandleRotation (which
    // has a 2x multiplier for large angles, causing jarring snaps on patrol).
    const FVector MoveDelta = FVector(NewLocation.X - PrevFramePos.X,
                                       NewLocation.Y - PrevFramePos.Y, 0.f);
    const float MoveDist = MoveDelta.Size();
    PrevFramePos = NewLocation;

    if (MoveDist > 0.5f && CurrentInterpSpeed > StoppedSpeedThreshold)
    {
        const FVector MoveDirection = MoveDelta / MoveDist;
        SmoothedFacingDir = FMath::VInterpNormalRotationTo(
            SmoothedFacingDir, MoveDirection, DeltaTime, PatrolRotationRate);

        ACharacter* Character = Cast<ACharacter>(GetOwner());
        if (Character)
        {
            FRotator DesiredRot = SmoothedFacingDir.Rotation();
            DesiredRot.Pitch = 0.f;
            DesiredRot.Roll  = 0.f;
            Character->SetActorRotation(
                FMath::RInterpConstantTo(
                    Character->GetActorRotation(), DesiredRot,
                    DeltaTime, PatrolRotationRate));
        }
    }

    // Walk animation: start when the mob has a meaningful target.
    if (!bIsMoving && GapToServer > MovingStartThreshold)
    {
        UpdateMovingState(true);
    }
    else if (bIsMoving && CurrentInterpSpeed < MovingStopThreshold)
    {
        UpdateMovingState(false);
    }
}

