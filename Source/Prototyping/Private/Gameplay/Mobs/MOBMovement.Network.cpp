// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include <Kismet/GameplayStatics.h>
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"

// ---------------------------------------------------------------------------
void UMOBMovementComponent::OnReceiveServerPacket(const FPositionDataStruct& MOBPosition)
{
    if (bFrozen) return;

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

    if (bFrozen) return;

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
            // BlendDuration scales with positional gap so the mob visually moves
            // at roughly patrol/return speed → prevents fast-blend "teleports" when
            // dead-reckoning drifted during RETURNING (state 5) or long chases.
            const FVector ClientPos = GetOwner()->GetActorLocation();
            const float TransitionGap = FVector::Dist2D(ClientPos, NewPos);
            const float SafeSpeed = FMath::Max(
                LatestServerSpeed > 0.f ? LatestServerSpeed : PatrolSpeed, 100.f);
            BlendFromPos  = ClientPos;
            BlendToPos    = NewPos;
            BlendElapsed  = 0.f;
            BlendDuration = FMath::Clamp(TransitionGap / SafeSpeed,
                                         StateTransitionBlendTime, 1.5f);
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
