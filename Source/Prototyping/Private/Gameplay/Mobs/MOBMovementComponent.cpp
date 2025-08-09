// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

// Sets default values for this component's properties
UMOBMovementComponent::UMOBMovementComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.
    PrimaryComponentTick.bCanEverTick = true;

    PrevServerPos = TargetServerPos = FVector::ZeroVector;
    PrevServerRot = TargetServerRot = FRotator::ZeroRotator;
    ServerVelocity = FVector::ZeroVector;
    LastMovePacketTime = 0.f;
    bHasVelocity = false;
}


// Called when the game starts
void UMOBMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize positions with owner's location if available
    if (GetOwner())
    {
        PrevServerPos = TargetServerPos = GetOwner()->GetActorLocation();
        PrevServerRot = TargetServerRot = GetOwner()->GetActorRotation();

        // Immediately snap to ground when spawned
        SnapToGround();
    }
}


// Called every frame
void UMOBMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Process movement if we have valid data
    if (bHasVelocity)
    {
        ProcessMovement(DeltaTime);
    }
    else if (GetOwner())
    {
        // Even when not moving, periodically check ground
        TimeSinceLastGroundCheck += DeltaTime;

        // Adjust every second when not moving
        if (TimeSinceLastGroundCheck >= 0.2f)
        {
            FVector AdjustedLocation = AdjustToGround(GetOwner()->GetActorLocation(), DeltaTime, 0.5f);
            GetOwner()->SetActorLocation(AdjustedLocation);
            TimeSinceLastGroundCheck = 0.0f;
        }
    }
}

void UMOBMovementComponent::OnReceiveServerPacket(const FPositionDataStruct& MOBPosition)
{
    // Get world reference for time/debug
    UWorld* World = GetWorld();
    if (!World) return;

    // Debug visualization if enabled
    if (bDebugGroundAdjustment)
    {
        DrawDebugSphere(World, FVector(MOBPosition.positionX, MOBPosition.positionY, MOBPosition.positionZ),
            20.0f, 12, FColor::Red, false, 5.0f);
    }

    float Now = World->GetTimeSeconds();

    // Create new position vector (keep current Z for now)
    FVector NewPos(MOBPosition.positionX,
        MOBPosition.positionY,
        GetOwner() ? GetOwner()->GetActorLocation().Z : MOBPosition.positionZ);

    //FRotator NewRot(0.f, MOBPosition.rotationZ, 0.f);

    // First position update - just set the position directly
    if (!bHasVelocity)
    {
        // Start interpolation immediately
        PrevServerPos = GetOwner() ? GetOwner()->GetActorLocation() : NewPos;
        TargetServerPos = NewPos;

        //PrevServerRot = GetActorRotation();
        //TargetServerRot = NewRot;

        ServerVelocity = FVector::ZeroVector;
        LastMovePacketTime = Now;
        bHasVelocity = true;

        // Make sure we're on the ground after first position
        SnapToGround();
    }
    else
    {
        // Calculate time delta since last packet
        float DeltaT = FMath::Max(Now - LastMovePacketTime, 0.016f); // Minimum 1/60 sec to avoid spikes
        LastMovePacketTime = Now;

        // Use CURRENT target as previous, instead of actual position
        // This prevents jerky movement when packets arrive late
        PrevServerPos = TargetServerPos;
        //PrevServerRot = TargetServerRot;

        // Update target position
        TargetServerPos = NewPos;
        //TargetServerRot = NewRot;

        // Calculate velocity with smoothing for more natural movement
        FVector NewVelocity = (TargetServerPos - PrevServerPos) / DeltaT;

        // Smooth velocity changes to prevent jerking
        // Use more aggressive smoothing for big velocity changes
        float VelocityMagnitudeDiff = FMath::Abs(NewVelocity.Size() - ServerVelocity.Size());
        float SmoothingFactor = FMath::Clamp(VelocityMagnitudeDiff / 300.f, 0.3f, 0.7f);

        // Blend between previous and new velocity
        ServerVelocity = FMath::Lerp(ServerVelocity, NewVelocity, SmoothingFactor);

        // Debug significant changes
        float MoveDist = FVector::Dist(PrevServerPos, TargetServerPos);
        if (MoveDist > 20.0f && bDebugGroundAdjustment)
        {
            UE_LOG(LogTemp, Verbose, TEXT("MOB Movement: dist: %.1f, speed: %.1f, time: %.3fs"),
                MoveDist, ServerVelocity.Size(), DeltaT);
        }
    }
}

FVector UMOBMovementComponent::AdjustToGround(const FVector& Location, float DeltaTime, float Priority)
{
    FVector AdjustedLocation = Location;
    UWorld* World = GetWorld();
    if (!World) return AdjustedLocation;

    // Get character and capsule
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return AdjustedLocation;

    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (!Capsule) return AdjustedLocation;

    // Start multiple traces for more stable ground detection
    const int32 NumTraces = 3;
    const float TraceSpacing = 30.0f; // Distance between traces
    TArray<float> GroundHeights;
    GroundHeights.Reserve(NumTraces);

    // Trace parameters
    float TraceHeight = 200.0f;
    float TraceDepth = 400.0f;

    // Create trace params that ignore this actor
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Character);
    Params.bTraceComplex = false;
    Params.bReturnPhysicalMaterial = false;

    // Perform multiple traces around the mob for better ground detection
    bool bFoundGround = false;
    float AverageGroundHeight = 0.0f;

    // Add the center point first
    FVector CenterStart = Location + FVector(0, 0, TraceHeight);
    FVector CenterEnd = Location - FVector(0, 0, TraceDepth);
    FHitResult CenterHit;

    if (World->LineTraceSingleByChannel(CenterHit, CenterStart, CenterEnd, ECC_WorldStatic, Params))
    {
        GroundHeights.Add(CenterHit.Location.Z);
        bFoundGround = true;

        if (bDebugGroundAdjustment)
        {
            DrawDebugLine(World, CenterStart, CenterHit.Location, FColor::Green, false, 0.05f);
            DrawDebugSphere(World, CenterHit.Location, 5.0f, 8, FColor::Red, false, 0.05f);
        }
    }
    else if (bDebugGroundAdjustment)
    {
        DrawDebugLine(World, CenterStart, CenterEnd, FColor::Red, false, 0.05f);
    }

    // Add additional traces in a small radius if the mob is moving
    // This helps prevent jitter when crossing uneven terrain
    if (bIsMoving)
    {
        // Get mob forward direction and right direction
        FVector Forward = Character->GetActorForwardVector();
        FVector Right = Character->GetActorRightVector();

        // Add traces in front and to the sides
        TArray<FVector> TraceOffsets;
        TraceOffsets.Add(Forward * TraceSpacing);  // Forward
        TraceOffsets.Add(-Forward * TraceSpacing); // Back
        TraceOffsets.Add(Right * TraceSpacing);    // Right
        TraceOffsets.Add(-Right * TraceSpacing);   // Left

        for (const FVector& Offset : TraceOffsets)
        {
            FVector TraceStart = Location + Offset + FVector(0, 0, TraceHeight);
            FVector TraceEnd = Location + Offset - FVector(0, 0, TraceDepth);
            FHitResult Hit;

            if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
            {
                GroundHeights.Add(Hit.Location.Z);
                bFoundGround = true;

                if (bDebugGroundAdjustment)
                {
                    DrawDebugLine(World, TraceStart, Hit.Location, FColor::Yellow, false, 0.05f);
                    DrawDebugSphere(World, Hit.Location, 3.0f, 8, FColor::Orange, false, 0.05f);
                }
            }
        }
    }

    // Calculate the target Z position based on the average ground height
    if (bFoundGround)
    {
        // Sort heights and take the median to avoid outliers
        if (GroundHeights.Num() > 2)
        {
            GroundHeights.Sort();
            AverageGroundHeight = GroundHeights[GroundHeights.Num() / 2]; // Median value
        }
        else
        {
            // Just use the average if we only have 1-2 points
            for (float Height : GroundHeights)
            {
                AverageGroundHeight += Height;
            }
            AverageGroundHeight /= GroundHeights.Num();
        }

        // Add capsule half height to get the proper actor Z position
        float DesiredZ = AverageGroundHeight + Capsule->GetScaledCapsuleHalfHeight();

        // Apply different smoothing rates based on whether we're going up or down
        // This prevents the "popping" effect when terrain changes
        float CurrentZ = Location.Z;
        float ZDifference = DesiredZ - CurrentZ;

        const float GroundZThreshold = 2.0f;
        if (FMath::Abs(ZDifference) < GroundZThreshold)
        {
            // Difference too small - keep current height
            AdjustedLocation.Z = CurrentZ;
            return AdjustedLocation;
        }

        // Use faster interpolation for downward movement (falling)
        // Use slower interpolation for upward movement (climbing)
        float InterpSpeed;
        if (ZDifference < 0)
        {
            // Going down - faster
            InterpSpeed = 15.0f * Priority;
        }
        else
        {
            // Going up - slower and smoother
            // Scale speed by how far we need to go up
            float UpFactor = FMath::Clamp(FMath::Abs(ZDifference) / 50.0f, 0.5f, 1.5f);
            InterpSpeed = 8.0f * Priority * UpFactor;
        }

        // Special case: if we're very far from the ground, move faster to catch up
        if (FMath::Abs(ZDifference) > 100.0f)
        {
            InterpSpeed *= 2.0f;
        }

        // Apply the vertical adjustment with the calculated interpolation speed
        AdjustedLocation.Z = FMath::FInterpTo(CurrentZ, DesiredZ, DeltaTime, InterpSpeed);

        // Debug visualization for target height
        if (bDebugGroundAdjustment)
        {
            DrawDebugSphere(World, FVector(AdjustedLocation.X, AdjustedLocation.Y, DesiredZ),
                8.0f, 8, FColor::Blue, false, 0.05f);
        }
    }

    return AdjustedLocation;
}

void UMOBMovementComponent::ProcessMovement(float DeltaTime)
{
    if (!GetOwner()) return;

    const FVector CurrentLocation = GetOwner()->GetActorLocation();
    const FVector TargetXY(TargetServerPos.X, TargetServerPos.Y, 0.f);
    const FVector CurrentXY(CurrentLocation.X, CurrentLocation.Y, 0.f);
    const float HorizontalDist = FVector::Dist(CurrentXY, TargetXY);

    // If close enough, handle with simple interpolation
    if (HorizontalDist <= SnapDistance)
    {
        HandleCloseRangeMovement(CurrentLocation, DeltaTime);
        return;
    }

    // Calculate and apply position update
    FVector NewLocation = CalculateMovementPosition(CurrentLocation, TargetXY, CurrentXY, HorizontalDist, DeltaTime);

    // Adjust to ground and set position
    const float Priority = FMath::Clamp(1.0f - (HorizontalDist / 500.0f), 0.3f, 1.0f);
    NewLocation = AdjustToGround(NewLocation, DeltaTime, Priority);
    GetOwner()->SetActorLocation(NewLocation);

    // Handle rotation separately
    HandleRotation(TargetXY - CurrentXY, DeltaTime);

    // Mark as moving
    UpdateMovingState(true);
}

void UMOBMovementComponent::HandleCloseRangeMovement(const FVector& CurrentLocation, float DeltaTime)
{
    FVector NewLoc = FMath::VInterpTo(CurrentLocation, TargetServerPos, DeltaTime, 20.0f);
    NewLoc = AdjustToGround(NewLoc, DeltaTime);
    GetOwner()->SetActorLocation(NewLoc);
    UpdateMovingState(FVector::Dist2D(CurrentLocation, TargetServerPos) > 1.f);
}

FVector UMOBMovementComponent::CalculateMovementPosition(
    const FVector& CurrentLocation,
    const FVector& TargetXY,
    const FVector& CurrentXY,
    float HorizontalDist,
    float DeltaTime)
{
    // Speed interpolation
    CurrentInterpSpeed = FMath::FInterpTo(CurrentInterpSpeed,
        FMath::Max(ServerVelocity.Size(), MinMoveSpeed), DeltaTime, 2.0f);

    // Movement direction
    FVector MoveDir = (TargetXY - CurrentXY).GetSafeNormal();
    FVector NewLocation = CurrentLocation + MoveDir * CurrentInterpSpeed * DeltaTime;

    // Prevent overshooting
    const FVector NewXY(NewLocation.X, NewLocation.Y, 0.f);
    if (FVector::Dist(NewXY, TargetXY) > HorizontalDist)
    {
        NewLocation.X = TargetServerPos.X;
        NewLocation.Y = TargetServerPos.Y;
    }

    return NewLocation;
}

void UMOBMovementComponent::HandleRotation(const FVector& MoveVector, float DeltaTime)
{
    if (MoveVector.IsNearlyZero()) return;

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;

    FRotator DesiredRot = MoveVector.Rotation();
    DesiredRot.Pitch = 0;
    DesiredRot.Roll = 0;

    const float AngleDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(
        Character->GetActorRotation().Yaw, DesiredRot.Yaw));

    const float TurnSpeed = FMath::Lerp(2.0f, 8.0f,
        FMath::Clamp(AngleDiff / 90.0f, 0.f, 1.f));

    Character->SetActorRotation(FMath::RInterpTo(
        Character->GetActorRotation(), DesiredRot, DeltaTime, TurnSpeed));
}

void UMOBMovementComponent::UpdateMovingState(bool bNewIsMoving)
{
    if (bIsMoving != bNewIsMoving)
    {
        bIsMoving = bNewIsMoving;

        // Notify owner about movement state change
        // Could use a delegate here if needed
    }
}

void UMOBMovementComponent::SnapToGround()
{
    if (!GetOwner()) return;

    // Perform a more aggressive ground adjustment for initial placement
    FVector CurrentLocation = GetOwner()->GetActorLocation();
    FVector AdjustedLocation = AdjustToGround(CurrentLocation, 0.016f, 2.0f);

    // Use immediate placement rather than interpolation
    GetOwner()->SetActorLocation(AdjustedLocation);

    if (bDebugGroundAdjustment)
    {
        UE_LOG(LogTemp, Warning, TEXT("MOB snapped to ground: Z adjustment = %f"),
            AdjustedLocation.Z - CurrentLocation.Z);
    }
}