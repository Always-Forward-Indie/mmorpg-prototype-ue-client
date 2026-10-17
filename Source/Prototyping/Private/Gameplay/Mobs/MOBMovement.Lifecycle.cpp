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
    CRASH_GUARD("MOBMovementComponent::Tick");

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

