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

    // AAA guard: a non-finite location (NaN/Inf from a bad spawn packet or
    // interpolation state) turns the Chaos traversal below into an unbounded
    // loop that wedges the game thread forever. Skip instead.
    if (!FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) || !FMath::IsFinite(Location.Z))
    {
        static bool bLoggedTrace = false;
        if (!bLoggedTrace)
        {
            bLoggedTrace = true;
            UE_LOG(LogTemp, Warning, TEXT("[MOB] TraceGround: non-finite location (%.1f,%.1f,%.1f) — skipping ground trace"), Location.X, Location.Y, Location.Z);
        }
        return FVector::ZeroVector;
    }

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

    // AAA guard: a non-finite location (NaN/Inf from a bad spawn packet or
    // interpolation state) turns the Chaos traversal below into an unbounded
    // loop that wedges the game thread forever. Skip instead.
    if (!FMath::IsFinite(Location.X) || !FMath::IsFinite(Location.Y) || !FMath::IsFinite(Location.Z))
    {
        static bool bLoggedAdjust = false;
        if (!bLoggedAdjust)
        {
            bLoggedAdjust = true;
            AActor* Owner = GetOwner();
            UE_LOG(LogTemp, Warning, TEXT("[MOB] AdjustToGround: non-finite location (%.1f,%.1f,%.1f) on '%s' — skipping ground adjustment"),
                Location.X, Location.Y, Location.Z, Owner ? *Owner->GetName() : TEXT("null-owner"));
        }
        return AdjustedLocation;
    }

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

    // Center trace — recorded separately so we can prefer it when descending off
    // an elevated surface (fence / ledge).  Surrounding probes may still "see"
    // the elevated surface within their ±30-unit offset radius even after the mob
    // center has moved past it, causing the median to stay too high.
    float CenterGroundZ = 0.f;
    bool  bCenterFound  = false;
    {
        const FVector Start = FVector(Location.X, Location.Y, Location.Z + GroundTraceHeight);
        const FVector End   = FVector(Location.X, Location.Y, Location.Z - GroundTraceDepth);
        FHitResult CenterHit;
        if (World->LineTraceSingleByObjectType(CenterHit, Start, End, ObjectParams, Params))
        {
            CenterGroundZ = CenterHit.Location.Z;
            bCenterFound  = true;
            GroundHeights.Add(CenterGroundZ);
            bFoundGround  = true;
            if (bDebugGroundAdjustment)
            {
                DrawDebugLine(World, Start, CenterHit.Location, FColor::Green, false, 0.05f);
            }
        }
    }

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

    // When descending: if the center trace finds ground significantly lower than the
    // multi-probe median (mob just stepped off a fence / ledge), trust the center
    // exclusively.  This prevents surrounding probes — which still "see" the elevated
    // surface — from holding the mob airborne after it has moved past the obstacle.
    float GroundToUse = MedianGround;
    if (bCenterFound)
    {
        const float CenterDesiredZ = CenterGroundZ + Capsule->GetScaledCapsuleHalfHeight();
        // Going down AND center is clearly lower than what surrounding probes found
        if (CenterDesiredZ < Location.Z && (MedianGround - CenterGroundZ) > CenterPriorityThreshold)
        {
            GroundToUse = CenterGroundZ;
        }
    }

    const float DesiredZ = GroundToUse + Capsule->GetScaledCapsuleHalfHeight();
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
        const float DownFactor = FMath::Clamp(FMath::Abs(ZDiff) / 40.0f, 1.0f, 6.0f);
        InterpSpeed = GroundInterpSpeedDown * Priority * DownFactor;
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
