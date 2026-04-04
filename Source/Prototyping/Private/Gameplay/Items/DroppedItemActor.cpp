// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
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

// Spawn a one-shot SFX with SoundClassOverride set BEFORE Play() Ч same pattern
// used across the whole codebase.  SpawnSoundAtLocation/PlaySoundAtLocation call
// Play() internally so any class override set afterwards is silently ignored.
static void PlayItemSFX(AActor* Owner, USoundBase* Sound)
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
			1.0f, 1.0f, 0.0f, nullptr, nullptr,
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
		UGameplayStatics::PlaySoundAtLocation(Owner, Sound, Owner->GetActorLocation());
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
	InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
	InteractionSphere->SetGenerateOverlapEvents(true);

	// Create and attach Niagara drop effect component (idle loop while on ground)
	DropNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DropNiagaraComponent"));
	DropNiagaraComponent->SetupAttachment(RootSceneComponent);
	DropNiagaraComponent->SetAutoActivate(false);

	bVisualsSetupComplete = false;
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
	//   1. Mob drop   Ч fly arc from mob location to ground
	//   2. Player drop Ч small toss arc from player's current position
	//   3. World/nearby Ч item already on the ground, just snap
	const bool bFromMob    = !ItemData.droppedByMobUID.IsEmpty();
	const bool bFromPlayer = !bFromMob && (ItemData.droppedByCharacterId > 0);

	UE_LOG(LogDropSnap, Log,
		TEXT("[DropSnap] BeginPlay source: %s"),
		bFromMob ? TEXT("MOB") : (bFromPlayer ? TEXT("PLAYER") : TEXT("WORLD/NEARBY")));

	if (bFromMob)
	{
		TryStartTrajectoryFromMob();
	}
	else
	{
		// Player drop or nearbyItems Ч run standard drop-from-above animation
		// (for nearbyItems the server Z is already ground-level so DropHeight stays small)
		const float GroundZ = FindGroundLevelAt(InitialPosition);

		// Update the target position with proper ground level
		TargetPosition = FVector(InitialPosition.X, InitialPosition.Y, GroundZ);

		UE_LOG(LogDropSnap, Log,
			TEXT("[DropSnap] Standard drop '%s' | InitialZ=%.1f -> GroundZ=%.1f | TargetPos=%s"),
			*ItemData.item.name, InitialPosition.Z, GroundZ, *TargetPosition.ToString());

		// Start the standard drop animation
		DropStartTime = GetGameTimeSinceCreation();
		bIsDropAnimationActive = true;

		// Calculate a random offset for drop direction
		FVector RandomDir = FVector(FMath::RandRange(-1.0f, 1.0f), FMath::RandRange(-1.0f, 1.0f), 0.0f);
		RandomDir.Normalize();
		DropHorizontalOffset = RandomDir * FMath::RandRange(30.0f, 50.0f);

		// Set initial Z offset for drop animation
		DropHeight = FMath::RandRange(80.0f, 120.0f);

		// Set the initial position to be above the target
		SetActorLocation(FVector(TargetPosition.X, TargetPosition.Y, TargetPosition.Z + DropHeight));

		// Set initial rotation based on VisualData settings
		if (ItemMesh)
		{
			if (VisualData.bUseCustomGroundRotation)
			{
				// Use custom ground rotation immediately
				ItemMesh->SetRelativeRotation(VisualData.GroundRotation);
				UE_LOG(LogTemp, Log, TEXT("Using custom ground rotation for item %s: %s"),
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
				UE_LOG(LogTemp, Log, TEXT("Using default rotation for item %s: %s"),
					*ItemData.item.name, *DefaultRotation.ToString());
			}
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

			// If dropping from mob position to target, use trajectory arc
			if (!ItemData.droppedByMobUID.IsEmpty())
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
			// Animation finished Ч snap precisely to the surface using the loaded mesh bounds
			bIsDropAnimationActive = false;
			SnapToGround();

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
	// BeginPlay, so we must not start the animation here Ч BeginPlay will do it.
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

	// данные визуала из твоего менеджера
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
			// Fallback по типу
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
			// оттенок по редкости
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
				PlayItemSFX(this, Cue);
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

	// Pickup SFX Ч routed through SFX SoundClass so the volume slider works
	if (!VisualData.PickupSound.IsNull())
	{
		if (USoundCue* Cue = VisualData.PickupSound.LoadSynchronous())
		{
			PlayItemSFX(this, Cue);
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

	Destroy();
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
		UE_LOG(LogDropSnap, Log, TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' Ч no mobUID, skipping"), *ItemData.item.name);
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
			FVector MobLocation = SourceMob->GetActorLocation();
			UE_LOG(LogDropSnap, Log,
				TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | mob found at %s"),
				*ItemData.item.name, *MobLocation.ToString());
			SetupTrajectoryAnimation(MobLocation);
		}
		else
		{
			UE_LOG(LogDropSnap, Warning,
				TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | actor found but Cast<ABasicMOB> failed Ч falling back to SnapToGround"),
				*ItemData.item.name);
			SnapToGround();
		}
	}
	else
	{
		UE_LOG(LogDropSnap, Warning,
			TEXT("[DropSnap] TryStartTrajectoryFromMob '%s' | mob '%s' NOT found in world Ч falling back to SnapToGround"),
			*ItemData.item.name, *ItemData.droppedByMobUID);
		SnapToGround();
	}
}

float ADroppedItemActor::FindGroundLevelAt(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World) return Location.Z;

	// Trace from a fixed high altitude so we reliably hit the ground
	// even when Location.Z is already at or below the surface.
	const FVector StartPos = FVector(Location.X, Location.Y, Location.Z + 5000.0f);
	const FVector EndPos   = FVector(Location.X, Location.Y, Location.Z - 5000.0f);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	QueryParams.bIgnoreTouches = true;

	// Always ignore self
	QueryParams.AddIgnoredActor(this);

	// Explicitly ignore the local player pawn Ч it may be standing exactly
	// at the drop point and its CapsuleComponent would be hit first.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* LocalPawn = PC->GetPawn())
		{
			QueryParams.AddIgnoredActor(LocalPawn);
		}
	}

	// Ignore all pawns (remote players, mobs, NPCs) and other dropped items
	TArray<AActor*> PawnsToIgnore;
	UGameplayStatics::GetAllActorsOfClass(World, ABasicPlayer::StaticClass(), PawnsToIgnore);
	UGameplayStatics::GetAllActorsOfClass(World, ABasicMOB::StaticClass(),    PawnsToIgnore);
	UGameplayStatics::GetAllActorsOfClass(World, ABasicNPC::StaticClass(),    PawnsToIgnore);
	UGameplayStatics::GetAllActorsOfClass(World, ADroppedItemActor::StaticClass(), PawnsToIgnore);
	QueryParams.AddIgnoredActors(PawnsToIgnore);

	// ECC_Visibility ignores trigger volumes and pawn capsules responding Overlap/Ignore,
	// while hitting real walkable geometry (landscape heightfield, static meshes, BSP).
	UE_LOG(LogDropSnap, Verbose,
		TEXT("[DropSnap] Trace '%s' | Start=%s End=%s"),
		*ItemData.item.name, *StartPos.ToString(), *EndPos.ToString());

	const bool bHit = World->LineTraceSingleByChannel(
		HitResult, StartPos, EndPos, ECC_Visibility, QueryParams);

	if (bHit && HitResult.bBlockingHit)
	{
		const float ResultZ = HitResult.ImpactPoint.Z + 1.0f;
		UE_LOG(LogDropSnap, Log,
			TEXT("[DropSnap] Trace HIT '%s' | ImpactZ=%.1f -> ResultZ=%.1f | Component='%s'"),
			*ItemData.item.name, HitResult.ImpactPoint.Z, ResultZ,
			HitResult.GetComponent() ? *HitResult.GetComponent()->GetName() : TEXT("null"));
		return ResultZ;
	}

	UE_LOG(LogDropSnap, Warning,
		TEXT("[DropSnap] Trace MISS '%s' | Location=%s Ч keeping server Z=%.1f"),
		*ItemData.item.name, *Location.ToString(), Location.Z);
	return Location.Z;
}

void ADroppedItemActor::SnapToGround()
{
	const FVector CurrentLoc = GetActorLocation();
	// FindGroundLevelAt internally adds ±5000 to the given Z for the trace range,
	// so pass CurrentLoc directly Ч no extra offset here.
	const float GroundZ = FindGroundLevelAt(CurrentLoc);

	// Work out how far the mesh pivot sits above its own bottom edge
	// so the lowest point of the mesh lands exactly on the surface.
	// We use the world-space bounding box of the mesh component directly Ч
	// this already accounts for RelativeScale3D, MeshScale and actor scale.
	float PivotToBottom = 0.0f;
	if (ItemMesh && ItemMesh->GetStaticMesh())
	{
		// Force recalculation so Bounds reflect the current world transform
		ItemMesh->UpdateBounds();
		const FBox WorldBox = ItemMesh->Bounds.GetBox();
		// Distance from the actor origin (pivot) to the bottom of the mesh in world Z
		PivotToBottom = CurrentLoc.Z - WorldBox.Min.Z;
		// Clamp: never go negative (pivot already below its own bottom is a degenerate case)
		PivotToBottom = FMath::Max(PivotToBottom, 0.0f);

		UE_LOG(LogDropSnap, Log,
			TEXT("[DropSnap] Bounds '%s' | BoxMin=%.1f BoxMax=%.1f BoxCenter=%.1f | PivotToBottom=%.1f"),
			*ItemData.item.name,
			WorldBox.Min.Z, WorldBox.Max.Z, WorldBox.GetCenter().Z,
			PivotToBottom);
	}
	else
	{
		UE_LOG(LogDropSnap, Warning,
			TEXT("[DropSnap] SnapToGround '%s' Ч no mesh/static mesh, PivotToBottom=0"),
			*ItemData.item.name);
	}

	const FVector SnappedLoc = FVector(CurrentLoc.X, CurrentLoc.Y, GroundZ + PivotToBottom);
	SetActorLocation(SnappedLoc, false, nullptr, ETeleportType::TeleportPhysics);

	UE_LOG(LogDropSnap, Log,
		TEXT("[DropSnap] Snapped '%s' | CurrentZ=%.1f GroundZ=%.1f PivotToBottom=%.1f -> FinalZ=%.1f"),
		*ItemData.item.name, CurrentLoc.Z, GroundZ, PivotToBottom, SnappedLoc.Z);
}