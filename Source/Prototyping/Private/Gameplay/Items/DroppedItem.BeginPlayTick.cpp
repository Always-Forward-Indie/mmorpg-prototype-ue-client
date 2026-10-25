// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/DroppedItemActor.h"
#include "Prototyping.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
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

void ADroppedItemActor::BeginPlay()
{
	Super::BeginPlay();

	InitialPosition = GetActorLocation();

	UE_LOG(LogDropSnap, Log,
		TEXT("[DropSnap] BeginPlay '%s' (uid=%d, itemId=%d, slug='%s') | spawnLoc=%s | serverPos=(%.1f, %.1f, %.1f) | mobUID='%s' | charId=%d"),
		*ItemData.item.name, ItemData.uid, ItemData.item.id, *ItemData.item.slug,
		*InitialPosition.ToString(),
		ItemData.position.positionX, ItemData.position.positionY, ItemData.position.positionZ,
		*ItemData.droppedByMobUID, ItemData.droppedByCharacterId);

	// Setup visuals based on the item data
	if (!bVisualsSetupComplete)
	{
		SetupItemVisuals_Implementation();
	}

	// Determine drop source and choose the correct animation path:
	//   1. Mob drop   пїЅ fly arc from mob location to ground
	//   2. Player drop пїЅ small toss arc from player's current position
	//   3. World/nearby пїЅ item already on the ground, just snap
	const bool bFromMob    = !ItemData.droppedByMobUID.IsEmpty();
	const bool bFromPlayer = !bFromMob && (ItemData.droppedByCharacterId > 0);

	UE_LOG(LogDropSnap, Log,
		TEXT("[DropSnap] BeginPlay source: %s"),
		bFromMob ? TEXT("MOB") : (bFromPlayer ? TEXT("PLAYER") : TEXT("WORLD/NEARBY")));

	if (bFromMob)
	{
		TryStartTrajectoryFromMob();
	}
	else if (bFromPlayer)
        {
                bIsPlayerDrop = true;
                FVector LaunchOrigin(
                        ItemData.position.positionX,
                        ItemData.position.positionY,
                        ItemData.position.positionZ);
                SetupTrajectoryAnimation(LaunchOrigin);

                // FindGroundLevelAt may miss if the landscape doesn't block ECC_WorldStatic.
                // Use the dropper's actual foot Z instead -- both clients have the dropper
                // actor in the world, so this is always reliable and identical everywhere.
                if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
                {
                        if (ABasicPlayer* Dropper = GI->GetPlayerByCharacterId(ItemData.droppedByCharacterId))
                        {
                                const float FootZ = Dropper->GetActorLocation().Z
                                        - Dropper->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                                TargetPosition.Z = FootZ;
                                UE_LOG(LogDropSnap, Log,
                                        TEXT("[DropSnap] Player drop '%s' -- TargetPosition.Z = dropper foot Z=%.1f"),
                                        *ItemData.item.name, FootZ);
                        }
                }
        }
	else
	{
		// nearbyItems / world items вЂ” already on the ground.
		// Skip the drop animation and snap directly so items don't pop in mid-air.
		UE_LOG(LogDropSnap, Log,
			TEXT("[DropSnap] nearbyItem '%s' вЂ” snapping to landscape, ignoring server Z=%.1f"),
			*ItemData.item.name, InitialPosition.Z);
		SnapToGround();

		// Schedule a second snap 0.5s later: landscape collision may be still
		// streaming in at BeginPlay time, causing the first trace to miss.
		GetWorldTimerManager().SetTimer(
			DelayedSnapTimerHandle, this, &ADroppedItemActor::SnapToGround, 0.5f, false);

		if (ItemMesh)
		{
			if (VisualData.bUseCustomGroundRotation)
				ItemMesh->SetRelativeRotation(VisualData.GroundRotation);
			else
				ItemMesh->SetRelativeRotation(FRotator(
					0.0f,
					FMath::RandRange(0.0f, 360.0f),
					0.0f));
		}
	}
}

// Called every frame
void ADroppedItemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Apply drop animation
	if (bIsDropAnimationActive)
	{
		const float TimeSinceDrop = GetGameTimeSinceCreation() - DropStartTime;
		const float DropDuration = 0.8f; // Make drop from mob a bit longer

		if (TimeSinceDrop < DropDuration)
		{
			// Calculate animation progress (0.0 to 1.0)
			float Progress = FMath::Clamp(TimeSinceDrop / DropDuration, 0.0f, 1.0f);

			// Use EaseOut curve for smoother landing
			float EasedProgress = 1.0f - FMath::Pow(1.0f - Progress, 2.0f);

			// Calculate position along the trajectory
			FVector CurrentPosition;

			// If dropping from mob/player position to target, use arc trajectory.
			// bIsPlayerDrop is set in BeginPlay for player drops so they use the same
			// formula as mob drops вЂ” arc from InitialPosition (server spawn point) to
			// TargetPosition (ground). This keeps the visual identical on all clients.
			if (!ItemData.droppedByMobUID.IsEmpty() || bIsPlayerDrop)
			{
				// Lerp from initial to target position
				CurrentPosition = FMath::Lerp(InitialPosition, TargetPosition, EasedProgress);

				// Add vertical arc component - higher at the middle of the trajectory
				float ArcHeight = DropHeight * FMath::Sin(EasedProgress * PI);
				CurrentPosition.Z += ArcHeight;

				// Add horizontal offset to make it more natural
				FVector HorizontalOffset = DropHorizontalOffset * FMath::Sin(EasedProgress * PI);
				CurrentPosition += HorizontalOffset;
			}
			// Standard drop from above
			else
			{
				// Calculate current height based on a parabolic arc
				float CurrentHeight = DropHeight * (1.0f - EasedProgress);

				// Calculate horizontal position (item moves slightly as it falls)
				FVector HorizontalOffset = DropHorizontalOffset * (1.0f - EasedProgress);

				// Set new position
				CurrentPosition = TargetPosition + HorizontalOffset + FVector(0, 0, CurrentHeight);
			}

			// Apply the calculated position
			SetActorLocation(CurrentPosition);

			// Note: Rotation animation removed - rotation is now set immediately based on VisualData settings
		}
		else
		{
			// Animation finished вЂ” snap precisely to the surface using the loaded mesh bounds.
			// Schedule a delayed retry (same pattern as nearby items): the first SnapToGround
			// may fail if the landscape complex-collision chunk wasn't streamed in yet at the
			// moment the trajectory started (causing FindGroundLevelAt to return the server Z
			// ~93 instead of the real surface Z ~0).  The 0.5 s retry catches this race.
			bIsDropAnimationActive = false;
			SnapToGround();
			GetWorldTimerManager().SetTimer(
				DelayedSnapTimerHandle, this, &ADroppedItemActor::SnapToGround, 0.5f, false);

			if (ItemMesh)
			{
				if (VisualData.bUseCustomGroundRotation)
				{
					// Force disable physics simulation if it's on
					bool wasSimulating = ItemMesh->IsSimulatingPhysics();
					if (wasSimulating)
					{
						ItemMesh->SetSimulatePhysics(false);
					}

					// Apply the custom ground rotation as relative rotation
					ItemMesh->SetRelativeRotation(VisualData.GroundRotation, false, nullptr, ETeleportType::ResetPhysics);

					// Log the actual rotation after setting it
					UE_LOG(LogGameplay, Warning, TEXT("Final rotation set for item %s - Target: %s, Actual Relative: %s, Actual World: %s"),
						*ItemData.item.name,
						*VisualData.GroundRotation.ToString(),
						*ItemMesh->GetRelativeRotation().ToString(),
						*ItemMesh->GetComponentRotation().ToString());

					// Restore physics state if needed
					if (wasSimulating)
					{
						ItemMesh->SetSimulatePhysics(true);
					}
				}
				else
				{
					// Default behavior - set neutral rotation with minimal pitch and roll variation
					FRotator DefaultRotation = FRotator(
						FMath::RandRange(-5.0f, 5.0f),  // Small pitch variation
						FMath::RandRange(0.0f, 360.0f), // Random yaw
						FMath::RandRange(-5.0f, 5.0f)   // Small roll variation
					);
					ItemMesh->SetRelativeRotation(DefaultRotation);
					UE_LOG(LogGameplay, Warning, TEXT("Using default rotation for item %s: %s"),
						*ItemData.item.name, *DefaultRotation.ToString());
				}
			}

			UE_LOG(LogGameplay, Warning, TEXT("Drop animation complete for item %s"), *ItemData.item.name);
		}
	}
}

