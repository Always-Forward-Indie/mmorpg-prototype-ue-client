// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include <Kismet/GameplayStatics.h>
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"

void UMOBMovementComponent::ProcessCombatMovement(float DeltaTime)
{
    if (!GetOwner()) return;

    const FVector CurrentLocation = GetOwner()->GetActorLocation();

    // Attack states (2-4): mob is stationary
    if (CombatState >= 2 && CombatState <= 4)
    {
        FVector GroundLoc = AdjustToGround(CurrentLocation, DeltaTime, 1.0f);
        GetOwner()->SetActorLocation(GroundLoc);
        PrevFramePos   = GroundLoc;
        CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed, 0.f, DeltaTime, 8.f);
        UpdateMovingState(false);
        bBlendActive = false;
        return;
    }

    const bool bStopped = (LatestServerSpeed < StoppedSpeedThreshold);

    if (bStopped)
    {
        // Server says mob is stopped — settle toward last known server pos
        FVector SettleLoc = FMath::VInterpTo(CurrentLocation,
            FVector(LatestServerPos.X, LatestServerPos.Y, CurrentLocation.Z),
            DeltaTime, 8.f);
        SettleLoc = AdjustToGround(SettleLoc, DeltaTime);
        GetOwner()->SetActorLocation(SettleLoc);
        PrevFramePos   = SettleLoc;
        CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed, 0.f, DeltaTime, 8.f);
        UpdateMovingState(false);
        return;
    }

    // Extrapolation cap: if no movement packet arrived within ExtrapolationMaxTime,
    // stop dead-reckoning and gently settle toward the last known server position.
    // Root cause of "mob runs away forever": when the server stops broadcasting
    // (e.g. mob is waitingForMeleeSlot / blocked), TimeSinceLastPacket grows
    // unboundedly and dead reckoning runs indefinitely at LatestServerSpeed.
    if (TimeSinceLastPacket >= ExtrapolationMaxTime)
    {
        FVector SettleLoc = FMath::VInterpTo(CurrentLocation,
            FVector(LatestServerPos.X, LatestServerPos.Y, CurrentLocation.Z),
            DeltaTime, 4.f);
        SettleLoc = AdjustToGround(SettleLoc, DeltaTime);
        GetOwner()->SetActorLocation(SettleLoc);
        PrevFramePos       = SettleLoc;
        CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed, 0.f, DeltaTime, 8.f);
        UpdateMovingState(false);
        return;
    }

    // --- Dead Reckoning + Proportional Correction -----------------------------
    // Instead of the old blend-from/blend-to system that restarted every packet
    // (causing micro-stutters), we continuously dead-reckon forward and gently
    // correct toward the estimated server position.

    // 1. Smooth movement direction to dampen server-side deflection avoidance.
    //    720°/s is fast enough to track genuine turns but prevents the ±30-90°
    //    single-packet deflections from visually flipping the mob sideways.
    if (!LatestServerDir.IsNearlyZero())
    {
        DeadReckonDir = FMath::VInterpNormalRotationTo(
            DeadReckonDir, LatestServerDir, DeltaTime, 720.f);
    }

    // 2. Dead reckon: move in smoothed direction at server speed.
    //    This is the primary movement driver — continuous, no restarts.
    const FVector DeadReckonStep(
        DeadReckonDir.X * LatestServerSpeed * DeltaTime,
        DeadReckonDir.Y * LatestServerSpeed * DeltaTime,
        0.f);
    FVector NewLocation(
        CurrentLocation.X + DeadReckonStep.X,
        CurrentLocation.Y + DeadReckonStep.Y,
        CurrentLocation.Z);

    // 3. Estimated server position via extrapolation (for correction target).
    //    Use raw LatestServerDir (not smoothed) for accurate server estimate.
    //    Cap at 0.5s — generous enough that normal 100ms jitter never freezes,
    //    but prevents runaway drift if packets genuinely stop.
    const float ExtrapTime = FMath::Min(TimeSinceLastPacket, 0.5f);
    const FVector EstimatedServerPos(
        LatestServerPos.X + LatestServerDir.X * LatestServerSpeed * ExtrapTime,
        LatestServerPos.Y + LatestServerDir.Y * LatestServerSpeed * ExtrapTime,
        CurrentLocation.Z);

    // 4. Proportional correction toward estimated server position.
    //    This closes any gap from direction smoothing or cumulative drift
    //    without the jarring blend restarts of the old system.
    const float Gap = FVector::Dist2D(NewLocation, EstimatedServerPos);
    if (Gap > 1.f)
    {
        const FVector CorrDir = (EstimatedServerPos - NewLocation).GetSafeNormal2D();
        const float CorrRate = FMath::Clamp(Gap * 3.f, 0.f, LatestServerSpeed * 0.5f);
        const float CorrStep = FMath::Min(CorrRate * DeltaTime, Gap);
        NewLocation.X += CorrDir.X * CorrStep;
        NewLocation.Y += CorrDir.Y * CorrStep;
    }

    // 5. Smooth speed for animation
    CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed, LatestServerSpeed, DeltaTime, 10.f);

    NewLocation = AdjustToGround(NewLocation, DeltaTime, 1.0f);
    GetOwner()->SetActorLocation(NewLocation);

    // 6. Rotation: derive from actual frame-to-frame movement delta.
    //    This avoids jitter from server deflection avoidance — the mob's visual
    //    facing follows its actual screen-space path, not the raw server direction.
    const FVector MoveDelta = FVector(NewLocation.X - PrevFramePos.X,
                                       NewLocation.Y - PrevFramePos.Y, 0.f);
    const float MoveDist = MoveDelta.Size();
    PrevFramePos = NewLocation;

    if (MoveDist > 1.f)
    {
        const FVector ActualDir = MoveDelta / MoveDist;
        SmoothedFacingDir = FMath::VInterpNormalRotationTo(
            SmoothedFacingDir, ActualDir, DeltaTime, 540.f);
        HandleRotation(SmoothedFacingDir, DeltaTime);
    }

    // 7. Moving state
    if (!bIsMoving && CurrentInterpSpeed > MovingStartThreshold)
    {
        UpdateMovingState(true);
    }
    else if (bIsMoving && CurrentInterpSpeed < MovingStopThreshold)
    {
        UpdateMovingState(false);
    }
}

// ---------------------------------------------------------------------------
// PATROL: smooth interpolation toward server-authoritative position.
//
// The server moves patrol mobs in discrete jumps (every 2-6 seconds) and
// reports the new position + waypoint + speed. The client smoothly moves
// toward the latest server position at PatrolSpeed. Between server jumps
// (when no packets arrive), the mob gently drifts toward the waypoint to
// maintain visual continuity. This eliminates the old extrapolation-based
// bouncing where the client predicted a phantom server position that diverged
// from reality.
