// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/DroppedItemActor.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogDropSnap, Log, All);

// Spawn a one-shot SFX with SoundClassOverride set BEFORE Play() — same pattern
// used across the whole codebase.  SpawnSoundAtLocation/PlaySoundAtLocation call
// Play() internally so any class override set afterwards is silently ignored.
static void PlayItemSFX(AActor* Owner, USoundBase* Sound, USoundAttenuation* Attenuation = nullptr)
{
	if (!Owner || !Sound) { return; }

	USoundClass* SFXClass = nullptr;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
	{
		if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
	}

	if (SFXClass)
	{
		UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
			Sound, Owner->GetRootComponent(), NAME_None,
			Owner->GetActorLocation(), FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition,
			/*bStopWhenAttachedToDestroyed=*/true,
			1.0f, 1.0f, 0.0f, Attenuation, nullptr,
			/*bAutoActivate=*/false);
		if (AC)
		{
			AC->SoundClassOverride = SFXClass;
			AC->bAutoDestroy = true;
			AC->Play();
		}
	}
	else
	{
		UGameplayStatics::SpawnSoundAttached(Sound, Owner->GetRootComponent(), NAME_None,
			Owner->GetActorLocation(), FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
			true, 1.0f, 1.0f, 0.0f, Attenuation, nullptr, true);
	}
}

// Sets default values
ADroppedItemActor::ADroppedItemActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create and setup root component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	// Create and attach mesh component
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(RootSceneComponent);
	ItemMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ItemMesh->SetGenerateOverlapEvents(true);
	ItemMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));

	// Create and attach interaction sphere
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootSceneComponent);
	InteractionSphere->SetSphereRadius(PickupRadius);
	// Block ECC_Visibility so the cursor hover trace can detect this dropped item.
	InteractionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
	InteractionSphere->SetGenerateOverlapEvents(true);

	// Create and attach Niagara drop effect component (idle loop while on ground)
	DropNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DropNiagaraComponent"));
	DropNiagaraComponent->SetupAttachment(RootSceneComponent);
	DropNiagaraComponent->SetAutoActivate(false);

	// Cursor target-indicator decal (floor circle).
	TargetDecal = CreateDefaultSubobject<UTargetDecalComponent>(TEXT("TargetDecal"));
	TargetDecal->SetupAttachment(RootSceneComponent);

	bVisualsSetupComplete = false;
}

// ─── IWorldInteractable interface ────────────────────────────────────────────
EInteractableType ADroppedItemActor::GetInteractableType() const
{
    return EInteractableType::DroppedItem;
}

FText ADroppedItemActor::GetInteractableDisplayName() const
{
    const int32 Rarity = GetItemRarity();
    if (Rarity > 0)
        return FText::FromString(FString::Printf(TEXT("%s  (%d)"), *GetItemName(), Rarity));
    return FText::FromString(GetItemName());
}

bool ADroppedItemActor::CanInteract() const
{
    return ItemData.canBePickedUp;
}

// Called when the game starts or when spawned
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
					UE_LOG(LogTemp, Warning, TEXT("Final rotation set for item %s - Target: %s, Actual Relative: %s, Actual World: %s"),
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
					UE_LOG(LogTemp, Warning, TEXT("Using default rotation for item %s: %s"),
						*ItemData.item.name, *DefaultRotation.ToString());
				}
			}

			UE_LOG(LogTemp, Warning, TEXT("Drop animation complete for item %s"), *ItemData.item.name);
		}
	}
}

void ADroppedItemActor::SetItemData(const FDroppedItemStruct& InItemData)
{
	ItemData = InItemData;

	// Visuals and trajectory are initialised in BeginPlay once the actor is
	// fully spawned. SetItemData is called via SpawnActorDeferred BEFORE
	// BeginPlay, so we must not start the animation here пїЅ BeginPlay will do it.
	// If SetItemData is called AFTER BeginPlay (live update), handle it then.
	if (HasActorBegunPlay())
	{
		if (!bVisualsSetupComplete)
		{
			SetupItemVisuals_Implementation();
		}

		if (!ItemData.droppedByMobUID.IsEmpty())
		{
			TryStartTrajectoryFromMob();
		}
	}
}

int32 ADroppedItemActor::GetItemRarity() const
{
	// Look for a "rarity" attribute
	for (const FItemAttributeStruct& Attribute : ItemData.item.attributes)
	{
		if (Attribute.slug == "rarity" || Attribute.name.ToLower() == "rarity")
		{
			return FMath::Max(1, FMath::Min(5, static_cast<int32>(Attribute.value)));
		}
	}

	// Default to common (1) if no rarity found
	return 1;
}

void ADroppedItemActor::SetupItemVisuals_Implementation()
{
	if (!ItemMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ItemMesh component is null in DroppedItemActor"));
		return;
	}

	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get GameInstance in DroppedItemActor"));
		return;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogTemp, Error, TEXT("Item manager not found in DroppedItemActor"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Setting up visuals for item %s (Slug: %s)"),
		*ItemData.item.name, *ItemData.item.slug);

	// пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
	VisualData = ItemManager->GetItemVisualDataBySlug(ItemData.item.slug);

	// ---------- Mesh ----------
	bool bAppliedMesh = false;
	{
		const FSoftObjectPath MeshPath = VisualData.ItemMesh.ToSoftObjectPath();
		UE_LOG(LogTemp, Log, TEXT("Loot Mesh path: %s"), *MeshPath.ToString());

		if (MeshPath.IsValid())
		{
			if (UStaticMesh* Mesh = VisualData.ItemMesh.LoadSynchronous())
			{
				ItemMesh->SetStaticMesh(Mesh);
				ItemMesh->SetRelativeScale3D(VisualData.MeshScale);
				bAppliedMesh = true;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to LoadSynchronous mesh: %s"), *MeshPath.ToString());
			}
		}

		if (!bAppliedMesh)
		{
			// Fallback пїЅпїЅ пїЅпїЅпїЅпїЅ
			FString FallbackPath;
			switch (ItemData.item.itemType)
			{
			case EItemType::Weapon:     FallbackPath = TEXT("/Engine/BasicShapes/Cube.Cube");      break;
			case EItemType::Armor:      FallbackPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); break;
			case EItemType::Consumable: FallbackPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");  break;
			case EItemType::Resource:   FallbackPath = TEXT("/Engine/BasicShapes/Cone.Cone");      break;
			default:                    FallbackPath = TEXT("/Engine/BasicShapes/Cube.Cube");      break;
			}

			if (UStaticMesh* DefaultMesh =
				Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *FallbackPath)))
			{
				ItemMesh->SetStaticMesh(DefaultMesh);
				ItemMesh->SetRelativeScale3D(FVector(0.5f));
				UE_LOG(LogTemp, Warning, TEXT("Applied fallback mesh: %s"), *FallbackPath);
			}
		}
	}

	// ---------- Material ----------
	{
		const FSoftObjectPath MatPath = VisualData.CustomMaterial.ToSoftObjectPath();
		UE_LOG(LogTemp, Log, TEXT("Loot Material path: %s"), *MatPath.ToString());

		if (MatPath.IsValid())
		{
			if (UMaterialInterface* Mat = VisualData.CustomMaterial.LoadSynchronous())
			{
				ItemMesh->SetMaterial(0, Mat);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to LoadSynchronous material: %s"), *MatPath.ToString());
			}
		}
		else
		{
			// пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
			const int32 Rarity = GetItemRarity();
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ItemMesh->GetMaterial(0), this))
			{
				FLinearColor Color = FLinearColor::White;
				switch (Rarity)
				{
				case 2: Color = FLinearColor::Green; break;
				case 3: Color = FLinearColor::Blue; break;
				case 4: Color = FLinearColor(0.5f, 0.f, 0.5f); break;
				case 5: Color = FLinearColor(1.f, 0.5f, 0.f); break;
				}
				MID->SetVectorParameterValue(TEXT("Color"), Color);
				ItemMesh->SetMaterial(0, MID);
			}
		}
	}

	// ---------- Drop Niagara (idle loop on ground) ----------
	if (DropNiagaraComponent)
	{
		const FSoftObjectPath FXPath = VisualData.DropNiagaraSystem.ToSoftObjectPath();
		UE_LOG(LogTemp, Log, TEXT("Loot DropNiagara path: %s"), *FXPath.ToString());

		if (FXPath.IsValid())
		{
			if (UNiagaraSystem* NS = VisualData.DropNiagaraSystem.LoadSynchronous())
			{
				DropNiagaraComponent->SetAsset(NS);
				DropNiagaraComponent->Activate(true);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to LoadSynchronous DropNiagaraSystem: %s"), *FXPath.ToString());
			}
		}
	}

	// ---------- Drop SFX ----------
	{
		const FSoftObjectPath SndPath = VisualData.DropSound.ToSoftObjectPath();
		UE_LOG(LogTemp, Log, TEXT("Loot DropSound path: %s"), *SndPath.ToString());

		if (SndPath.IsValid())
		{
			if (USoundCue* Cue = VisualData.DropSound.LoadSynchronous())
			{
				USoundAttenuation* Atn = VisualData.DefaultAttenuation.LoadSynchronous();
				PlayItemSFX(this, Cue, Atn);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to LoadSynchronous sound: %s"), *SndPath.ToString());
			}
		}
	}

	OnVisualsSetup();
	UE_LOG(LogTemp, Log, TEXT("SetupItemVisuals completed for item %s (Slug: %s)"),
		*ItemData.item.name, *ItemData.item.slug);
}


bool ADroppedItemActor::AttemptPickup()
{
	if (!ItemData.canBePickedUp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item %s (ID:%d) cannot be picked up"), *ItemData.item.name, ItemData.uid);
		return false;
	}

	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("Game instance not found"));
		return false;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogTemp, Error, TEXT("Item manager not found"));
		return false;
	}

	// Send pickup request to the server
	ItemManager->SendPickUpItemRequest(ItemData.uid);

	return true;
}

void ADroppedItemActor::Interact()
{
	// When interacted with, attempt to pick up the item
	AttemptPickup();
}

void ADroppedItemActor::OnVisualsSetup()
{
	bVisualsSetupComplete = true;
}

void ADroppedItemActor::PlayPickupEffect()
{
	// Stop and hide the idle drop effect
	if (DropNiagaraComponent)
	{
		DropNiagaraComponent->Deactivate();
	}

	// Pickup SFX пїЅ routed through SFX SoundClass so the volume slider works
	if (!VisualData.PickupSound.IsNull())
	{
		if (USoundCue* Cue = VisualData.PickupSound.LoadSynchronous())
		{
			USoundAttenuation* Atn = VisualData.DefaultAttenuation.LoadSynchronous();
			PlayItemSFX(this, Cue, Atn);
		}
	}

	// Spawn one-shot pickup Niagara at the item location
	const FSoftObjectPath FXPath = VisualData.PickupNiagaraSystem.ToSoftObjectPath();
	if (FXPath.IsValid())
	{
		if (UNiagaraSystem* NS = VisualData.PickupNiagaraSystem.LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				NS,
				GetActorLocation(),
				GetActorRotation(),
				FVector(1.f),
				/*bAutoDestroy=*/true,
				/*bAutoActivate=*/true,
				ENCPoolMethod::AutoRelease
			);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PlayPickupEffect: Failed to load PickupNiagaraSystem: %s"), *FXPath.ToString());
		}
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetLifeSpan(0.5f);
}

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

	UE_LOG(LogTemp, Log, TEXT("Item %s target position adjusted to ground level: %s (Ground Z: %.2f)"),
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
			UE_LOG(LogTemp, Log, TEXT("Using custom ground rotation for trajectory item %s: %s"),
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
			UE_LOG(LogTemp, Log, TEXT("Using default rotation for trajectory item %s: %s"),
				*ItemData.item.name, *DefaultRotation.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Starting drop trajectory for item %s from mob position %s to ground position %s"),
		*ItemData.item.name, *InitialPosition.ToString(), *TargetPosition.ToString());
}

void ADroppedItemActor::TryStartTrajectoryFromMob()
{
	if (ItemData.droppedByMobUID.IsEmpty())
	{
		UE_LOG(LogDropSnap, Log, TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' пїЅ no mobUID, skipping"), *ItemData.item.name);
		return;
	}

	// Find the mob that dropped this item
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(*ItemData.droppedByMobUID), FoundActors);

	UE_LOG(LogDropSnap, Log,
		TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | mobUID='%s' | foundActors=%d"),
		*ItemData.item.name, *ItemData.droppedByMobUID, FoundActors.Num());

	if (FoundActors.Num() > 0)
	{
		ABasicMOB* SourceMob = Cast<ABasicMOB>(FoundActors[0]);
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
				TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | actor found but Cast<ABasicMOB> failed пїЅ falling back to SnapToGround"),
				*ItemData.item.name);
			SnapToGround();
		}
	}
	else
	{
		UE_LOG(LogDropSnap, Warning,
			TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | mob '%s' NOT found in world пїЅ falling back to SnapToGround"),
			*ItemData.item.name, *ItemData.droppedByMobUID);
		SnapToGround();
	}
}

float ADroppedItemActor::FindGroundLevelAt(const FVector& Location)
{
UWorld* World = GetWorld();
if (!World) return Location.Z;

const FVector StartPos = FVector(Location.X, Location.Y,  50000.0f);
const FVector EndPos   = FVector(Location.X, Location.Y, -10000.0f);

FCollisionQueryParams QueryParams;
QueryParams.bTraceComplex = true;
QueryParams.bIgnoreTouches = true;

QueryParams.AddIgnoredActor(this);

if (APlayerController* PC = World->GetFirstPlayerController())
{
if (APawn* LocalPawn = PC->GetPawn())
{
QueryParams.AddIgnoredActor(LocalPawn);
}
}

TArray<AActor*> PawnsToIgnore;
UGameplayStatics::GetAllActorsOfClass(World, ABasicPlayer::StaticClass(), PawnsToIgnore);
UGameplayStatics::GetAllActorsOfClass(World, ABasicMOB::StaticClass(),    PawnsToIgnore);
UGameplayStatics::GetAllActorsOfClass(World, ABasicNPC::StaticClass(),    PawnsToIgnore);
UGameplayStatics::GetAllActorsOfClass(World, ADroppedItemActor::StaticClass(), PawnsToIgnore);
QueryParams.AddIgnoredActors(PawnsToIgnore);

TArray<FHitResult> Hits;
const bool bHit = World->LineTraceMultiByChannel(Hits, StartPos, EndPos, ECC_WorldStatic, QueryParams);

if (bHit)
{
float FirstShapeHitZ = TNumericLimits<float>::Lowest();

for (const FHitResult& Hit : Hits)
{
if (!Hit.bBlockingHit) continue;

if (AActor* HitActor = Hit.GetActor())
{
if (HitActor->IsA<APawn>())
{
UE_LOG(LogDropSnap, Warning,
TEXT("[DropSnap] Trace SKIP PAWN '%s' | Pawn='%s' at Z=%.1f"),
*ItemData.item.name, *HitActor->GetName(), Hit.ImpactPoint.Z);
continue;
}
}

const UPrimitiveComponent* HitComp = Hit.GetComponent();
if (HitComp && HitComp->IsA<UShapeComponent>())
{
if (Hit.ImpactPoint.Z > FirstShapeHitZ)
FirstShapeHitZ = Hit.ImpactPoint.Z;
continue;
}

const float ResultZ = Hit.ImpactPoint.Z + 1.0f;
UE_LOG(LogDropSnap, Log,
TEXT("[DropSnap] Trace HIT '%s' | ImpactZ=%.1f -> ResultZ=%.1f | Component='%s'"),
*ItemData.item.name, Hit.ImpactPoint.Z, ResultZ,
HitComp ? *HitComp->GetName() : TEXT("null"));
return ResultZ;
}

if (FirstShapeHitZ > TNumericLimits<float>::Lowest())
{
UE_LOG(LogDropSnap, Warning,
TEXT("[DropSnap] Trace SHAPE-FALLBACK '%s' | no geometry hit, using ShapeZ=%.1f"),
*ItemData.item.name, FirstShapeHitZ);
return FirstShapeHitZ + 1.0f;
}
}

UE_LOG(LogDropSnap, Warning,
TEXT("[DropSnap] Trace MISS '%s' | Location=%s | keeping server Z=%.1f"),
*ItemData.item.name, *Location.ToString(), Location.Z);
return Location.Z;
}

void ADroppedItemActor::SnapToGround()
{
const FVector CurrentLoc = GetActorLocation();
const float GroundZ = FindGroundLevelAt(CurrentLoc);

float PivotToBottom = 0.0f;
if (ItemMesh && ItemMesh->GetStaticMesh())
{
ItemMesh->UpdateBounds();
const FBox WorldBox = ItemMesh->Bounds.GetBox();
PivotToBottom = CurrentLoc.Z - WorldBox.Min.Z;
PivotToBottom = FMath::Max(PivotToBottom, 0.0f);

UE_LOG(LogDropSnap, Log,
TEXT("[DropSnap] Bounds '%s' | BoxMin=%.1f BoxMax=%.1f BoxCenter=%.1f | PivotToBottom=%.1f"),
*ItemData.item.name,
WorldBox.Min.Z, WorldBox.Max.Z, WorldBox.GetCenter().Z,
PivotToBottom);
}

const FVector SnappedLoc = FVector(CurrentLoc.X, CurrentLoc.Y, GroundZ + PivotToBottom);
SetActorLocation(SnappedLoc, false, nullptr, ETeleportType::TeleportPhysics);

UE_LOG(LogDropSnap, Log,
TEXT("[DropSnap] Snapped '%s' | CurrentZ=%.1f GroundZ=%.1f PivotToBottom=%.1f -> FinalZ=%.1f"),
*ItemData.item.name, CurrentLoc.Z, GroundZ, PivotToBottom, SnappedLoc.Z);
}