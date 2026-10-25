// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/DroppedItemActor.h"
#include "Prototyping.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ShapeComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Audio/AudioManager.h"
#include "Gameplay/Items/DroppedItemSFX.h"

void ADroppedItemActor::SetupTrajectoryAnimation(const FVector& SourceLocation)
{
	// Get the item's target position from FDroppedItemStruct (X,Y coordinates)
	FVector TargetXY(
		ItemData.position.positionX,
		ItemData.position.positionY,
		ItemData.position.positionZ // We'll override this Z value
	);

	// Find the ground level at the target position with a line trace
	float GroundZ = FindGroundLevelAt(TargetXY);

	// Store the target position with ground-aligned Z coordinate
	TargetPosition = FVector(TargetXY.X, TargetXY.Y, GroundZ);

	UE_LOG(LogGameplay, Verbose, TEXT("Item %s target position adjusted to ground level: %s (Ground Z: %.2f)"),
		*ItemData.item.name, *TargetPosition.ToString(), GroundZ);

	// Calculate a random horizontal offset for the arc's apex
	FVector RandomDir = FVector(FMath::RandRange(-1.0f, 1.0f), FMath::RandRange(-1.0f, 1.0f), 0.0f);
	RandomDir.Normalize();
	DropHorizontalOffset = RandomDir * FMath::RandRange(30.0f, 50.0f);

	// Set the initial position to be at the mob's location
	InitialPosition = SourceLocation;
	SetActorLocation(InitialPosition);

	// Calculate the drop height (the maximum height of the arc)
	// Use a higher value for a more pronounced arc
	DropHeight = FMath::RandRange(100.0f, 200.0f);

	// Start the drop animation
	DropStartTime = GetGameTimeSinceCreation();
	bIsDropAnimationActive = true;

	// Set initial rotation based on VisualData settings
	if (ItemMesh)
	{
		if (VisualData.bUseCustomGroundRotation)
		{
			// Use custom ground rotation immediately
			ItemMesh->SetRelativeRotation(VisualData.GroundRotation);
			UE_LOG(LogGameplay, Verbose, TEXT("Using custom ground rotation for trajectory item %s: %s"),
				*ItemData.item.name, *VisualData.GroundRotation.ToString());
		}
		else
		{
			// Apply default rotation with small variations
			FRotator DefaultRotation = FRotator(
				FMath::RandRange(-5.0f, 5.0f),   // Small pitch variation
				FMath::RandRange(0.0f, 360.0f),  // Random yaw
				FMath::RandRange(-5.0f, 5.0f)    // Small roll variation
			);
			ItemMesh->SetRelativeRotation(DefaultRotation);
			UE_LOG(LogGameplay, Verbose, TEXT("Using default rotation for trajectory item %s: %s"),
				*ItemData.item.name, *DefaultRotation.ToString());
		}
	}

	UE_LOG(LogGameplay, Verbose, TEXT("Starting drop trajectory for item %s from mob position %s to ground position %s"),
		*ItemData.item.name, *InitialPosition.ToString(), *TargetPosition.ToString());
}

void ADroppedItemActor::TryStartTrajectoryFromMob()
{
	if (ItemData.droppedByMobUID.IsEmpty())
	{
		UE_LOG(LogDropSnap, Log, TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' пїЅ no mobUID, skipping"), *ItemData.item.name);
		return;
	}

	// Find the mob that dropped this item (O(1) registry lookup)
	ABasicMOB* SourceMob = nullptr;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (UMOBManager* MobMgr = GI->GetMOBManager())
		{
			SourceMob = Cast<ABasicMOB>(MobMgr->FindMobActor(FCString::Atoi(*ItemData.droppedByMobUID)));
		}
	}

	UE_LOG(LogDropSnap, Log,
		TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | mobUID='%s' | found=%d"),
		*ItemData.item.name, *ItemData.droppedByMobUID, SourceMob != nullptr);

	if (SourceMob)
	{
			// Use the center of the mob's skeletal mesh bounding box as the throw origin.
			// GetActorLocation() returns the capsule center which may differ from the mesh center.
			FVector MobLocation = SourceMob->GetActorLocation();
			if (USkeletalMeshComponent* MobMesh = SourceMob->GetMesh())
			{
				MobMesh->UpdateBounds();
				MobLocation.Z = MobMesh->Bounds.GetBox().GetCenter().Z;
			}
			UE_LOG(LogDropSnap, Log,
				TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | mob found, launch origin Z=%.1f"),
				*ItemData.item.name, MobLocation.Z);
			SetupTrajectoryAnimation(MobLocation);
		}
		else
		{
			UE_LOG(LogDropSnap, Warning,
				TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | mob '%s' NOT found in registry - falling back to SnapToGround"),
				*ItemData.item.name, *ItemData.droppedByMobUID);
			SnapToGround();
		}
}

