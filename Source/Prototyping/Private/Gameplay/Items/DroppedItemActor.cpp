// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Items/ItemManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

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

	// Create and attach particle system
	ItemParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ItemParticles"));
	ItemParticles->SetupAttachment(RootSceneComponent);
	ItemParticles->SetAutoActivate(true);

	bVisualsSetupComplete = false;
}

// Called when the game starts or when spawned
void ADroppedItemActor::BeginPlay()
{
	Super::BeginPlay();

	InitialPosition = GetActorLocation();

	// Setup visuals based on the item data
	if (!bVisualsSetupComplete)
	{
		SetupItemVisuals_Implementation();
	}

	// Only start the standard drop animation if we don't have a mob source
	// This prevents double-animating when TryStartTrajectoryFromMob is called
	if (ItemData.droppedByMobUID.IsEmpty())
	{
		// Find ground level at the initial position
		float GroundZ = FindGroundLevelAt(InitialPosition);

		// Update the target position with proper ground level
		TargetPosition = FVector(InitialPosition.X, InitialPosition.Y, GroundZ);

		UE_LOG(LogTemp, Log, TEXT("Standard drop: adjusted target position to ground level: %s"),
			*TargetPosition.ToString());

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
	// If we have a mob UID, try to start trajectory animation
	else if (!ItemData.droppedByMobUID.IsEmpty())
	{
		TryStartTrajectoryFromMob();
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
			// Animation finished, set final position and rotation
			bIsDropAnimationActive = false;
			SetActorLocation(TargetPosition);

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

	// If already in the world, update visuals
	if (IsValid(this) && !bVisualsSetupComplete)
	{
		SetupItemVisuals_Implementation();
	}

	// If we have a mob UID, try to start trajectory animation
	if (!ItemData.droppedByMobUID.IsEmpty())
	{
		TryStartTrajectoryFromMob();
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

	// ---------- Particles ----------
	if (ItemParticles)
	{
		const FSoftObjectPath FXPath = VisualData.ItemParticleSystem.ToSoftObjectPath();
		UE_LOG(LogTemp, Log, TEXT("Loot ParticleSystem path: %s"), *FXPath.ToString());

		if (FXPath.IsValid())
		{
			if (UParticleSystem* PS = VisualData.ItemParticleSystem.LoadSynchronous())
			{
				ItemParticles->SetTemplate(PS);
				ItemParticles->SetVisibility(true);
				ItemParticles->Activate(true);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to LoadSynchronous particle system: %s"), *FXPath.ToString());
			}
		}
		else
		{
			const int32 Rarity = GetItemRarity();
			ItemParticles->SetVisibility(Rarity > 1);
			const float ParticleScale = 0.5f + (Rarity * 0.1f);
			ItemParticles->SetRelativeScale3D(FVector(ParticleScale));
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
				UGameplayStatics::PlaySoundAtLocation(this, Cue, GetActorLocation());
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
		return;
	}

	// Find the mob that dropped this item
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(*ItemData.droppedByMobUID), FoundActors);

	if (FoundActors.Num() > 0)
	{
		ABasicMOB* SourceMob = Cast<ABasicMOB>(FoundActors[0]);
		if (SourceMob)
		{
			// Get the mob's location as the source
			FVector MobLocation = SourceMob->GetActorLocation();

			// Set up the trajectory animation from the mob's position to the final drop position
			SetupTrajectoryAnimation(MobLocation);
		}
	}
}

float ADroppedItemActor::FindGroundLevelAt(const FVector& Location)
{
	// Start position for the trace (high above the target point)
	FVector StartPos = FVector(Location.X, Location.Y, Location.Z + 1000.0f);

	// End position for the trace (deep below the target point)
	FVector EndPos = FVector(Location.X, Location.Y, Location.Z - 1000.0f);

	// Setup trace parameters
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;

	// Ignore this actor and all other dropped items by their class
	QueryParams.AddIgnoredActor(this);

	// Set up object types to query - only include static world geometry (landscape, static meshes)
	// This automatically ignores dynamic actors like DroppedItemActors and Mobs
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic); // Only hit static world geometry

	// Debug visualization
	bool bDebugTrace = false; // Set to true to visualize the trace in-game

	// Perform the line trace
	bool bHit = false;

	if (bDebugTrace)
	{
		// Draw a debug line to visualize the trace
		FColor TraceColor = FColor::Red;
		FColor HitColor = FColor::Green;
		float DebugLifetime = 5.0f;

		bHit = GetWorld()->LineTraceSingleByObjectType(
			HitResult,
			StartPos,
			EndPos,
			ObjectParams,
			QueryParams
		);

		DrawDebugLine(
			GetWorld(),
			StartPos,
			bHit ? HitResult.ImpactPoint : EndPos,
			bHit ? HitColor : TraceColor,
			false,
			DebugLifetime,
			0,
			1.0f
		);

		if (bHit)
		{
			DrawDebugPoint(
				GetWorld(),
				HitResult.ImpactPoint,
				10.0f,
				HitColor,
				false,
				DebugLifetime,
				0
			);
		}
	}
	else
	{
		// Standard trace without debug drawing
		bHit = GetWorld()->LineTraceSingleByObjectType(
			HitResult,
			StartPos,
			EndPos,
			ObjectParams,
			QueryParams
		);
	}

	// If we hit something, return the Z coordinate of the hit location
	if (bHit && HitResult.bBlockingHit)
	{
		// Add a small offset to prevent Z-fighting/clipping
		const float SurfaceOffset = 1.0f;

		if (bDebugTrace && HitResult.GetActor())
		{
			UE_LOG(LogTemp, Log, TEXT("Ground detection for item %s: Hit %s at Z=%.2f"),
				*ItemData.item.name, *HitResult.GetActor()->GetName(), HitResult.ImpactPoint.Z);
		}

		return HitResult.ImpactPoint.Z + SurfaceOffset;
	}

	// If nothing was hit, fall back to the original Z position
	UE_LOG(LogTemp, Warning, TEXT("Ground detection failed for item %s at location %s. Using original Z value."),
		*ItemData.item.name, *Location.ToString());
	return Location.Z;
}