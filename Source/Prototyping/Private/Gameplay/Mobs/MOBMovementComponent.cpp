// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include <Kismet/GameplayStatics.h>
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"

// ---------------------------------------------------------------------------
UMOBMovementComponent::UMOBMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    LatestServerPos = FVector::ZeroVector;
    LatestServerDir = FVector::ZeroVector;
    Waypoint = FVector::ZeroVector;
    LastMovePacketTime = 0.f;
    bHasReceivedPacket = false;
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner())
    {
        const FVector StartPos = GetOwner()->GetActorLocation();
        LatestServerPos = StartPos;
        PrevFramePos = StartPos;
        SmoothedFacingDir = GetOwner()->GetActorForwardVector();
        DeadReckonDir = SmoothedFacingDir;
        SnapToGround();
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Mob is dead — do not move the corpse.
    if (bFrozen) return;

    if (bHasReceivedPacket)
    {
        TimeSinceLastPacket += DeltaTime;
        ProcessMovement(DeltaTime);
    }
    else if (GetOwner())
    {
        TimeSinceLastGroundCheck += DeltaTime;
        if (TimeSinceLastGroundCheck >= 0.25f)
        {
            FVector Loc = GetOwner()->GetActorLocation();
            FVector Ground = TraceGround(Loc);
            if (!Ground.IsZero())
            {
                Loc.Z = FMath::FInterpTo(Loc.Z, Ground.Z, DeltaTime, GroundInterpSpeedDown);
                GetOwner()->SetActorLocation(Loc);
            }
            TimeSinceLastGroundCheck = 0.0f;
        }
    }

    // Attack states (2-4): mob is stationary — rotate toward target actor.
    if (bEnableTargetTracking && CurrentTargetId != 0 &&
        CombatState >= 2 && CombatState <= 4)
    {
        UpdateTargetTracking(DeltaTime);
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::InitializeFromSpawnData(const FVector& SpawnPos,
                                                     const FVector& VelocityDir,
                                                     float Speed,
                                                     int32 InCombatState)
{
    LatestServerPos   = SpawnPos;
    LatestServerDir   = VelocityDir.GetSafeNormal2D();
    LatestServerSpeed = Speed;
    PatrolSpeed       = Speed;
    CombatState       = InCombatState;
    PrevCombatState   = InCombatState;
    PrevFramePos      = SpawnPos;
    bHasReceivedPacket = true;
    bBlendActive       = false;
    TimeSinceLastPacket = 0.f;

    if (!LatestServerDir.IsNearlyZero())
    {
        SmoothedFacingDir = LatestServerDir;
        DeadReckonDir = LatestServerDir;
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::OnReceiveServerPacket(const FPositionDataStruct& MOBPosition)
{
    UWorld* World = GetWorld();
    if (!World || !GetOwner()) return;

    if (bDebugGroundAdjustment)
    {
        DrawDebugSphere(World, FVector(MOBPosition.positionX, MOBPosition.positionY, MOBPosition.positionZ),
            20.0f, 12, FColor::Red, false, 5.0f);
    }

    const float Now = World->GetTimeSeconds();

    FVector NewPos(MOBPosition.positionX,
        MOBPosition.positionY,
        GetOwner()->GetActorLocation().Z);

    if (!bHasReceivedPacket)
    {
        LatestServerPos    = NewPos;
        LatestServerDir    = FVector::ZeroVector;
        LatestServerSpeed  = 0.f;
        PatrolSpeed        = 0.f;
        LastMovePacketTime = Now;
        TimeSinceLastPacket = 0.f;
        bHasReceivedPacket = true;
        bBlendActive       = false;
        PrevFramePos       = NewPos;
        SnapToGround();
    }
    else
    {
        LastMovePacketTime  = Now;
        TimeSinceLastPacket = 0.f;
        LatestServerPos     = NewPos;

        if (CombatState != 0)
        {
            BlendFromPos  = GetOwner()->GetActorLocation();
            BlendToPos    = NewPos;
            BlendElapsed  = 0.f;
            BlendDuration = BlendToServerTime;
            bBlendActive  = true;
        }
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::OnReceiveMovePacket(const FMobMoveEntryStruct& MoveEntry, int64 ServerSendMs, int64 ClientRecvMs)
{
    PrevCombatState = CombatState;
    CombatState     = MoveEntry.combatState;

    UWorld* World = GetWorld();
    if (!World || !GetOwner()) return;

    const float Now = World->GetTimeSeconds();

    const FVector NewPos(MoveEntry.position.positionX,
                         MoveEntry.position.positionY,
                         GetOwner()->GetActorLocation().Z);

    // Direction + speed directly from packet
    if (MoveEntry.speed > 0.f)
    {
        LatestServerDir   = FVector(MoveEntry.velocityX, MoveEntry.velocityY, 0.f).GetSafeNormal2D();
        LatestServerSpeed = MoveEntry.speed;
    }
    else
    {
        LatestServerDir   = FVector::ZeroVector;
        LatestServerSpeed = 0.f;
    }

    bHasWaypoint = MoveEntry.bHasWaypoint;
    if (bHasWaypoint)
    {
        Waypoint = FVector(MoveEntry.waypointX, MoveEntry.waypointY, 0.f);
    }

    LastStepTimestampMs    = MoveEntry.stepTimestampMs;
    LastPacketClientRecvMs = ClientRecvMs;
    LastMovePacketTime     = Now;
    TimeSinceLastPacket    = 0.f;

    // --- Teleport check -------------------------------------------------------
    const float DistToNew = FVector::Dist2D(GetOwner()->GetActorLocation(), NewPos);
    if (DistToNew > TeleportThreshold)
    {
        GetOwner()->SetActorLocation(NewPos);
        LatestServerPos        = NewPos;
        PatrolSpeed            = LatestServerSpeed;
        bPatrolReachedWaypoint = false;
        bBlendActive           = false;
        bHasReceivedPacket     = true;
        PrevFramePos           = NewPos;
        SmoothedFacingDir      = LatestServerDir.IsNearlyZero() ? GetOwner()->GetActorForwardVector() : LatestServerDir;
        DeadReckonDir          = SmoothedFacingDir;
        SnapToGround();
        return;
    }

    // --- First packet ever ---------------------------------------------------
    if (!bHasReceivedPacket)
    {
        GetOwner()->SetActorLocation(NewPos);
        LatestServerPos    = NewPos;
        PatrolSpeed        = LatestServerSpeed;
        bBlendActive       = false;
        bHasReceivedPacket = true;
        PrevFramePos       = NewPos;
        if (!LatestServerDir.IsNearlyZero())
        {
            SmoothedFacingDir = LatestServerDir;
            DeadReckonDir = LatestServerDir;
        }
        SnapToGround();
        return;
    }

    // --- PATROLLING (state 0) ------------------------------------------------
    if (CombatState == 0)
    {
        LatestServerPos        = NewPos;
        PatrolSpeed            = LatestServerSpeed;
        bPatrolReachedWaypoint = false;

        // Pre-seed facing direction from the packet so the rotate-in-place
        // phase in ProcessPatrolMovement is short or absent.  Without this
        // the mob can face a stale direction (e.g. ForwardVector at spawn)
        // and stall for half a second turning before it starts walking.
        if (!LatestServerDir.IsNearlyZero())
        {
            SmoothedFacingDir = FMath::VInterpNormalRotationTo(
                SmoothedFacingDir, LatestServerDir, 1.f, 360.f);
        }

        if (PrevCombatState != 0)
        {
            // Combat → patrol: blend for smooth visual transition.
            const FVector ClientPos = GetOwner()->GetActorLocation();
            BlendFromPos  = ClientPos;
            BlendToPos    = NewPos;
            BlendElapsed  = 0.f;
            BlendDuration = StateTransitionBlendTime;
            bBlendActive  = true;
        }
        // Normal patrol packets: ProcessPatrolMovement smoothly moves
        // the mob toward LatestServerPos. No blend needed.
        return;
    }

    // --- Combat / non-patrol states ------------------------------------------
    LatestServerPos = NewPos;

    const bool bStateChanged = (PrevCombatState != CombatState);
    if (bStateChanged && !LatestServerDir.IsNearlyZero())
    {
        // Snap dead-reckoning direction on state change for immediate response
        DeadReckonDir     = LatestServerDir;
        SmoothedFacingDir = LatestServerDir;
    }
    // Dead-reckoning + correction in ProcessCombatMovement handles
    // all positional smoothing. No blend system needed for combat.
}

// ---------------------------------------------------------------------------
// COMBAT: dead reckoning with proportional server-position correction.
//
// Strategy:
// 1. Smooth the server's movement direction to dampen deflection-avoidance
//    jitter (mobs steering ±30-90° to avoid each other).
// 2. Dead-reckon forward in the smoothed direction at server speed.
// 3. Correct toward the estimated server position proportionally to gap size.
// 4. Derive rotation from actual frame-to-frame delta (not server direction).
// 5. No freeze on missed packets — dead reckoning continues naturally.
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
void UMOBMovementComponent::ProcessPatrolMovement(float DeltaTime)
{
    if (!GetOwner()) return;

    const FVector CurrentLocation = GetOwner()->GetActorLocation();

    // --- Active blend from state transition (e.g. combat -> patrol) -----------
    if (bBlendActive)
    {
        BlendElapsed += DeltaTime;
        const float Alpha = FMath::Clamp(BlendElapsed / FMath::Max(BlendDuration, 0.001f), 0.f, 1.f);
        FVector BlendedPos = FMath::Lerp(
            FVector(BlendFromPos.X, BlendFromPos.Y, CurrentLocation.Z),
            FVector(LatestServerPos.X, LatestServerPos.Y, CurrentLocation.Z),
            Alpha);
        BlendedPos = AdjustToGround(BlendedPos, DeltaTime, 1.0f);
        GetOwner()->SetActorLocation(BlendedPos);
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

    // --- Stop condition (also covers idle / no waypoint) ---------------------
    // 5-unit hysteresis: once inside, stay until next packet resets state.
    if (!bHasWaypoint || PatrolSpeed < 1.f || GapToServer < 5.f)
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

// ---------------------------------------------------------------------------
void UMOBMovementComponent::FreezeMob()
{
    // Zero out all locomotion state so the corpse does not slide.
    LatestServerSpeed   = 0.f;
    LatestServerDir     = FVector::ZeroVector;
    DeadReckonDir       = FVector::ZeroVector;
    bFrozen             = true;
    UpdateMovingState(false);
}

// ---------------------------------------------------------------------------
FVector UMOBMovementComponent::TraceGround(const FVector& Location) const
{
    UWorld* World = GetWorld();
    if (!World) return FVector::ZeroVector;

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return FVector::ZeroVector;

    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (!Capsule) return FVector::ZeroVector;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Character);
    Params.bTraceComplex = false;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

    const FVector Start = Location + FVector(0, 0, GroundTraceHeight);
    const FVector End   = Location - FVector(0, 0, GroundTraceDepth);
    FHitResult Hit;

    if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, Params))
    {
        const float DesiredZ = Hit.Location.Z + Capsule->GetScaledCapsuleHalfHeight();
        return FVector(Location.X, Location.Y, DesiredZ);
    }

    return FVector::ZeroVector;
}

// ---------------------------------------------------------------------------
FVector UMOBMovementComponent::AdjustToGround(const FVector& Location, float DeltaTime, float Priority)
{
    FVector AdjustedLocation = Location;
    UWorld* World = GetWorld();
    if (!World) return AdjustedLocation;

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return AdjustedLocation;

    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (!Capsule) return AdjustedLocation;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Character);
    Params.bTraceComplex = false;
    Params.bReturnPhysicalMaterial = false;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

    TArray<float, TInlineAllocator<5>> GroundHeights;
    bool bFoundGround = false;

    auto DoTrace = [&](const FVector& Origin)
    {
        const FVector Start = FVector(Origin.X, Origin.Y, Location.Z + GroundTraceHeight);
        const FVector End   = FVector(Origin.X, Origin.Y, Location.Z - GroundTraceDepth);
        FHitResult HitResult;
        if (World->LineTraceSingleByObjectType(HitResult, Start, End, ObjectParams, Params))
        {
            GroundHeights.Add(HitResult.Location.Z);
            bFoundGround = true;
            if (bDebugGroundAdjustment)
            {
                DrawDebugLine(World, Start, HitResult.Location, FColor::Green, false, 0.05f);
            }
        }
    };

    DoTrace(Location);

    if (bIsMoving)
    {
        const float Spacing = 30.0f;
        const FVector Fwd = Character->GetActorForwardVector() * Spacing;
        const FVector Rgt = Character->GetActorRightVector() * Spacing;
        DoTrace(Location + Fwd);
        DoTrace(Location - Fwd);
        DoTrace(Location + Rgt);
        DoTrace(Location - Rgt);
    }

    if (!bFoundGround) return AdjustedLocation;

    float MedianGround;
    if (GroundHeights.Num() >= 3)
    {
        GroundHeights.Sort();
        MedianGround = GroundHeights[GroundHeights.Num() / 2];
    }
    else
    {
        float Sum = 0.f;
        for (float H : GroundHeights) Sum += H;
        MedianGround = Sum / GroundHeights.Num();
    }

    const float DesiredZ = MedianGround + Capsule->GetScaledCapsuleHalfHeight();
    const float CurrentZ = Location.Z;
    const float ZDiff    = DesiredZ - CurrentZ;

    if (FMath::Abs(ZDiff) < 1.5f)
    {
        AdjustedLocation.Z = CurrentZ;
        return AdjustedLocation;
    }

    float InterpSpeed;
    if (ZDiff < 0.f)
    {
        InterpSpeed = GroundInterpSpeedDown * Priority;
    }
    else
    {
        const float UpFactor = FMath::Clamp(FMath::Abs(ZDiff) / 50.0f, 0.5f, 1.5f);
        InterpSpeed = GroundInterpSpeedUp * Priority * UpFactor;
    }

    if (FMath::Abs(ZDiff) > 100.0f)
    {
        InterpSpeed *= 2.0f;
    }

    AdjustedLocation.Z = FMath::FInterpTo(CurrentZ, DesiredZ, DeltaTime, InterpSpeed);

    if (bDebugGroundAdjustment)
    {
        DrawDebugSphere(World, FVector(AdjustedLocation.X, AdjustedLocation.Y, DesiredZ),
            8.0f, 8, FColor::Blue, false, 0.05f);
    }

    return AdjustedLocation;
}

// ---------------------------------------------------------------------------
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

    FVector Loc = GetOwner()->GetActorLocation();
    FVector Ground = TraceGround(Loc);
    if (!Ground.IsZero())
    {
        Loc.Z = Ground.Z;
        GetOwner()->SetActorLocation(Loc);
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::SetTargetType(const FString& NewTargetType)
{
    if (CurrentTargetType != NewTargetType)
    {
        CurrentTargetType = NewTargetType;
        TimeSinceLastTargetUpdate = 0.0f;
    }
}

void UMOBMovementComponent::SetTargetId(int32 NewTargetId)
{
    if (CurrentTargetId != NewTargetId)
    {
        CurrentTargetId = NewTargetId;
        TimeSinceLastTargetUpdate = 0.0f;
    }
}

void UMOBMovementComponent::ClearTarget()
{
    CurrentTargetId = 0;
}

void UMOBMovementComponent::UpdateTargetTracking(float DeltaTime)
{
    AActor* TargetActor = FindTargetActor(CurrentTargetId, CurrentTargetType);
    if (TargetActor)
    {
        RotateTowardsTarget(TargetActor, DeltaTime);
    }
    else
    {
        ClearTarget();
    }
}

AActor* UMOBMovementComponent::FindTargetActor(int32 TargetId, FString TargetType)
{
    if (TargetId <= 0) return nullptr;

    UWorld* World = GetWorld();
    if (!World) return nullptr;

    TArray<AActor*> FoundActors;

    if (TargetType.Equals("Player", ESearchCase::IgnoreCase))
    {
        UGameplayStatics::GetAllActorsOfClass(World, ABasicPlayer::StaticClass(), FoundActors);
    }
    else if (TargetType.Equals("Mob", ESearchCase::IgnoreCase))
    {
        UGameplayStatics::GetAllActorsOfClass(World, ABasicMOB::StaticClass(), FoundActors);
    }
    else
    {
        return nullptr;
    }

    const FName TargetIdTag = FName(*FString::FromInt(TargetId));
    for (AActor* Actor : FoundActors)
    {
        if (Actor && Actor->ActorHasTag(TargetIdTag))
        {
            return Actor;
        }
    }

    return nullptr;
}

void UMOBMovementComponent::RotateTowardsTarget(AActor* TargetActor, float DeltaTime)
{
    if (!GetOwner() || !TargetActor) return;

    FVector DirectionToTarget = (TargetActor->GetActorLocation() - GetOwner()->GetActorLocation());
    DirectionToTarget.Z = 0.f;
    if (DirectionToTarget.SizeSquared() < 1.f) return;
    DirectionToTarget.Normalize();

    FRotator TargetRotation = DirectionToTarget.Rotation();
    TargetRotation.Pitch = 0.0f;
    TargetRotation.Roll = 0.0f;

    const FRotator CurrentRotation = GetOwner()->GetActorRotation();
    const float AngleDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw));
    if (AngleDiff <= MinAngleThreshold)
        return;

    GetOwner()->SetActorRotation(
        FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, AttackRotationSpeed));
}
