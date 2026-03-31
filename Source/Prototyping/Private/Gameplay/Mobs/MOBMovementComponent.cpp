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

    PrevServerPos = TargetServerPos = FVector::ZeroVector;
    PrevServerRot = TargetServerRot = FRotator::ZeroRotator;
    ServerVelocity = FVector::ZeroVector;
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
        PrevServerPos = TargetServerPos = GetOwner()->GetActorLocation();
        PrevServerRot = TargetServerRot = GetOwner()->GetActorRotation();
        SnapToGround();
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bHasReceivedPacket)
    {
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

    if (bEnableTargetTracking && CurrentTargetId != 0)
    {
        UpdateTargetTracking(DeltaTime);
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::OnReceiveServerPacket(const FPositionDataStruct& MOBPosition)
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (bDebugGroundAdjustment)
    {
        DrawDebugSphere(World, FVector(MOBPosition.positionX, MOBPosition.positionY, MOBPosition.positionZ),
            20.0f, 12, FColor::Red, false, 5.0f);
    }

    const float Now = World->GetTimeSeconds();

    FVector NewPos(MOBPosition.positionX,
        MOBPosition.positionY,
        GetOwner() ? GetOwner()->GetActorLocation().Z : MOBPosition.positionZ);

    if (!bHasReceivedPacket)
    {
        PrevServerPos = GetOwner() ? GetOwner()->GetActorLocation() : NewPos;
        TargetServerPos = NewPos;
        ServerVelocity = FVector::ZeroVector;
        ServerSpeed = 0.f;
        LastMovePacketTime = Now;
        bHasReceivedPacket = true;
        SnapToGround();
    }
    else
    {
        const float DeltaT = FMath::Max(Now - LastMovePacketTime, 0.016f);
        LastMovePacketTime = Now;
        PrevServerPos = TargetServerPos;
        TargetServerPos = NewPos;

        const FVector DerivedVel = (TargetServerPos - PrevServerPos) / DeltaT;
        ServerVelocity = FMath::VInterpTo(ServerVelocity, DerivedVel, DeltaT, 3.0f);
        ServerSpeed = ServerVelocity.Size2D();
    }
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::OnReceiveMovePacket(const FMobMoveEntryStruct& MoveEntry, int64 ServerSendMs, int64 ClientRecvMs)
{
    CombatState = MoveEntry.combatState;

    UWorld* World = GetWorld();
    if (!World || !GetOwner()) return;

    const float Now = World->GetTimeSeconds();

    FVector NewPos(MoveEntry.position.positionX,
                   MoveEntry.position.positionY,
                   GetOwner()->GetActorLocation().Z);

    const bool bHasServerVel = (MoveEntry.speed > 0.f);
    FVector PacketVelocity = FVector::ZeroVector;
    if (bHasServerVel)
    {
        PacketVelocity = FVector(MoveEntry.velocityX, MoveEntry.velocityY, 0.f) * MoveEntry.speed;
    }

    bHasWaypoint = MoveEntry.bHasWaypoint;
    if (bHasWaypoint)
    {
        Waypoint = FVector(MoveEntry.waypointX, MoveEntry.waypointY, 0.f);
    }

    LastStepTimestampMs = MoveEntry.stepTimestampMs;
    LastPacketClientRecvMs = ClientRecvMs;

    const float DistToNew = FVector::Dist2D(GetOwner()->GetActorLocation(), NewPos);
    if (DistToNew > TeleportThreshold)
    {
        GetOwner()->SetActorLocation(FVector(NewPos.X, NewPos.Y, GetOwner()->GetActorLocation().Z));
        PrevServerPos = TargetServerPos = NewPos;
        ServerVelocity = PacketVelocity;
        ServerSpeed = MoveEntry.speed;
        LastMovePacketTime = Now;
        bHasReceivedPacket = true;
        SnapToGround();
        return;
    }

    if (!bHasReceivedPacket)
    {
        PrevServerPos = GetOwner()->GetActorLocation();
        TargetServerPos = NewPos;
        ServerVelocity = PacketVelocity;
        ServerSpeed = MoveEntry.speed;
        LastMovePacketTime = Now;
        bHasReceivedPacket = true;
        SnapToGround();
    }
    else
    {
        const float DeltaT = FMath::Max(Now - LastMovePacketTime, 0.016f);
        LastMovePacketTime = Now;

        PrevServerPos = TargetServerPos;
        TargetServerPos = NewPos;

        if (bHasServerVel)
        {
            ServerVelocity = FMath::VInterpTo(ServerVelocity, PacketVelocity, DeltaT, 4.0f);
            ServerSpeed = FMath::FInterpTo(ServerSpeed, MoveEntry.speed, DeltaT, 4.0f);
        }
        else
        {
            const FVector DerivedVel = (TargetServerPos - PrevServerPos) / DeltaT;
            ServerVelocity = FMath::VInterpTo(ServerVelocity, DerivedVel, DeltaT, 3.0f);
            ServerSpeed = ServerVelocity.Size2D();
        }
    }
}

// ---------------------------------------------------------------------------
FVector UMOBMovementComponent::ComputeDeadReckonedTarget(float DeltaTime) const
{
    if (ServerSpeed < 1.0f)
    {
        return TargetServerPos;
    }

    FVector Predicted = TargetServerPos + ServerVelocity * DeltaTime;

    if (bHasWaypoint)
    {
        const float DistToWP = FVector::Dist2D(TargetServerPos, Waypoint);
        const float DistPredicted = FVector::Dist2D(TargetServerPos, Predicted);
        if (DistPredicted > DistToWP && DistToWP > 1.0f)
        {
            Predicted.X = Waypoint.X;
            Predicted.Y = Waypoint.Y;
        }
    }

    return Predicted;
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

    const FVector CurrentLocation = GetOwner()->GetActorLocation();
    const FVector DeadReckoned = ComputeDeadReckonedTarget(DeltaTime);

    const FVector TargetXY(DeadReckoned.X, DeadReckoned.Y, 0.f);
    const FVector CurrentXY(CurrentLocation.X, CurrentLocation.Y, 0.f);
    const float HorizontalDist = FVector::Dist(CurrentXY, TargetXY);

    if (HorizontalDist <= SnapDistance)
    {
        HandleCloseRangeMovement(CurrentLocation, DeltaTime);
        return;
    }

    FVector NewLocation = CalculateMovementPosition(CurrentLocation, TargetXY, CurrentXY, HorizontalDist, DeltaTime);

    NewLocation = AdjustToGround(NewLocation, DeltaTime, 1.0f);
    GetOwner()->SetActorLocation(NewLocation);

    const bool bInCombat = IsInCombatState() && CurrentTargetId != 0;
    if (!bInCombat)
    {
        HandleRotation(TargetXY - CurrentXY, DeltaTime);
    }

    UpdateMovingState(true);
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::HandleCloseRangeMovement(const FVector& CurrentLocation, float DeltaTime)
{
    FVector NewLoc = FMath::VInterpTo(CurrentLocation, TargetServerPos, DeltaTime, 20.0f);
    NewLoc = AdjustToGround(NewLoc, DeltaTime);
    GetOwner()->SetActorLocation(NewLoc);

    const float Remaining = FVector::Dist2D(CurrentLocation, TargetServerPos);
    if (Remaining < 1.f && ServerSpeed < 1.f)
    {
        UpdateMovingState(false);
        CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed, 0.f, DeltaTime, 6.0f);
    }
    else
    {
        UpdateMovingState(Remaining > 1.f);
    }
}

// ---------------------------------------------------------------------------
FVector UMOBMovementComponent::CalculateMovementPosition(
    const FVector& CurrentLocation,
    const FVector& TargetXY,
    const FVector& CurrentXY,
    float HorizontalDist,
    float DeltaTime)
{
    const float DesiredSpeed = FMath::Max(ServerSpeed, MinMoveSpeed);
    CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed, DesiredSpeed, DeltaTime, 4.0f);

    FVector MoveDir = (TargetXY - CurrentXY).GetSafeNormal();
    FVector NewLocation = CurrentLocation + MoveDir * CurrentInterpSpeed * DeltaTime;

    const FVector NewXY(NewLocation.X, NewLocation.Y, 0.f);
    if (FVector::Dist(NewXY, TargetXY) > HorizontalDist)
    {
        NewLocation.X = TargetXY.X;
        NewLocation.Y = TargetXY.Y;
    }

    return NewLocation;
}

// ---------------------------------------------------------------------------
void UMOBMovementComponent::HandleRotation(const FVector& MoveVector, float DeltaTime)
{
    if (MoveVector.IsNearlyZero()) return;

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;

    FRotator DesiredRot = MoveVector.Rotation();
    DesiredRot.Pitch = 0;
    DesiredRot.Roll = 0;

    Character->SetActorRotation(
        FMath::RInterpTo(Character->GetActorRotation(), DesiredRot, DeltaTime, MoveRotationSpeed));
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

    FVector DirectionToTarget = (TargetActor->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
    if (DirectionToTarget.IsNearlyZero()) return;

    FRotator TargetRotation = DirectionToTarget.Rotation();
    TargetRotation.Pitch = 0.0f;
    TargetRotation.Roll = 0.0f;

    FRotator CurrentRotation = GetOwner()->GetActorRotation();

    float AngleDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw));
    if (AngleDiff <= MinAngleThreshold)
        return;

    const float Speed = IsInCombatState() ? AttackRotationSpeed : (TargetTrackingSpeed / 45.0f);

    FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, Speed);
    GetOwner()->SetActorRotation(NewRotation);
}
