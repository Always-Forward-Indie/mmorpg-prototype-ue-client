// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataStructs.h"
#include "NiagaraSystem.h"
#include "Engine/SkeletalMesh.h"
#include "ItemStruct.generated.h"

// Different item types
UENUM(BlueprintType)
enum class EItemType : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Weapon = 1 UMETA(DisplayName = "Weapon"),
	Armor = 2 UMETA(DisplayName = "Armor"),
	Consumable = 3 UMETA(DisplayName = "Consumable"),
	Quest = 4 UMETA(DisplayName = "Quest Item"),
	Tool = 5 UMETA(DisplayName = "Tool"),
	Resource = 6 UMETA(DisplayName = "Resource")
};

// Base item struct
USTRUCT(BlueprintType)
struct FItemBaseStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString name = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString slug = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString description = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	EItemType itemType = EItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString itemTypeName = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FString itemTypeSlug = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	bool isQuestItem = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TArray<FItemAttributeStruct> attributes;
};

// Struct for dropped items in the world
USTRUCT(BlueprintType)
struct FDroppedItemStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	int32 uid = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	int32 itemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	FItemBaseStruct item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	FString droppedByMobUID = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	FPositionDataStruct position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	int32 quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	bool canBePickedUp = true;
};

// Response structure for item drop events
USTRUCT(BlueprintType)
struct FItemDropResponseStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Drop Response")
	TArray<FDroppedItemStruct> droppedItems;
};

USTRUCT(BlueprintType)
struct FItemVisualData : public FTableRowBase
{
	GENERATED_BODY()

	// The slug identifier that matches with FItemBaseStruct's slug
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	FString ItemSlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	// Reference to the mesh to use for this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<UStaticMesh> ItemMesh;

	// Reference to the particle system for this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<UParticleSystem> ItemParticleSystem;

	// Sound to play when the item is dropped
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<USoundCue> DropSound;

	// Scale to apply to the mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	FVector MeshScale = FVector(1.f, 1.f, 1.f);

	// Optional custom material override
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<UMaterialInterface> CustomMaterial;

	// Final rotation to apply when item is on the ground (in degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	FRotator GroundRotation = FRotator::ZeroRotator;

	// Whether to use the custom ground rotation instead of randomizing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	bool bUseCustomGroundRotation = false;

	// ---- Equipped visuals ----

	/** Static mesh used when this item is equipped on the character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	TSoftObjectPtr<UStaticMesh> EquippedStaticMesh;

	/** Skeletal mesh used when this item is equipped (e.g. animated weapons) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	TSoftObjectPtr<USkeletalMesh> EquippedSkeletalMesh;

	/** Socket on the character skeleton to attach the equipped mesh to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	FName EquipSocketName = NAME_None;

	/** Local-space transform applied to the equipped mesh after socket attachment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	FTransform EquippedRelativeTransform;

	/** Niagara VFX spawned and attached to the weapon socket during a swing anim */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	TSoftObjectPtr<UNiagaraSystem> EquippedSwingVFX;

	/** Niagara VFX that loops while the item is idle-equipped (e.g. flame on a torch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	TSoftObjectPtr<UNiagaraSystem> EquippedIdleVFX;

	/** Armor material type for impact sound lookup (e.g. "leather", "plate", "cloth") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	FName ArmorMaterialType = NAME_None;
};