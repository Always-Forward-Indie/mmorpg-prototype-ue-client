// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include <Kismet/GameplayStatics.h>
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "MyGameInstance.h"
#include "CrashDiagnostics.h"

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

    // O(1) registry lookup — replaces GetAllActorsOfClass + tag scan.
    UMyGameInstance* GI = Cast<UMyGameInstance>(World->GetGameInstance());
    UMOBManager* MobMgr = GI ? GI->GetMOBManager() : nullptr;
    if (!MobMgr) return nullptr;

    if (TargetType.Equals(TEXT("Player"), ESearchCase::IgnoreCase))
    {
        return MobMgr->FindPlayerActor(TargetId);
    }
    if (TargetType.Equals(TEXT("Mob"), ESearchCase::IgnoreCase))
    {
        return MobMgr->FindMobActor(TargetId);
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
