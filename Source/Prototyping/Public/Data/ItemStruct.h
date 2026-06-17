// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataStructs.h"
#include "NiagaraSystem.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundAttenuation.h"
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
	int32 droppedByCharacterId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	FPositionDataStruct position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	int32 quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	bool canBePickedUp = true;

	// Loot reservation: non-zero means only reservedForCharacterId may pick this up until reservation expires
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	int32 reservedForCharacterId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dropped Item")
	int64 reservationSecondsLeft = 0;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<UTexture2D> Icon;

	// Reference to the mesh to use for this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<UStaticMesh> ItemMesh;

	// Niagara effect that loops while the item is lying on the ground
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<UNiagaraSystem> DropNiagaraSystem;

	// Niagara effect played in the moment of pickup (one-shot, before actor is destroyed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<UNiagaraSystem> PickupNiagaraSystem;

	// Sound to play when the item is dropped
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<USoundCue> DropSound;

	// Sound to play when the item is picked up
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual")
	TSoftObjectPtr<USoundCue> PickupSound;

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

	/**
	 * Swing whoosh sound specific to this weapon type.
	 * Overrides FSkillDefinitionData::swingSound when this item is in the main_hand slot.
	 * Examples: sword = blade whoosh, staff = wooden swish, unarmed = fist whoosh (leave empty).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	TSoftObjectPtr<USoundBase> EquippedSwingSound;

	/** Niagara VFX that loops while the item is idle-equipped (e.g. flame on a torch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	TSoftObjectPtr<UNiagaraSystem> EquippedIdleVFX;

	/** Armor material type for impact sound lookup (e.g. "leather", "plate", "cloth") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Equipped")
	FName ArmorMaterialType = NAME_None;

	// ---- Audio ----

	/** Sound when this item is equipped onto the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Audio")
	TSoftObjectPtr<USoundBase> EquipSound;

	/** Sound when this item is unequipped from the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Audio")
	TSoftObjectPtr<USoundBase> UnequipSound;

	/** Sound when this item is used (consumed, activated). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Audio")
	TSoftObjectPtr<USoundBase> UseSound;

	/** Attenuation radius for 3D world sounds: DropSound, PickupSound, EquipSound, UnequipSound, UseSound.
	 *  Leave empty to rely on the SoundCue asset's own attenuation settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Visual|Audio")
	TSoftObjectPtr<USoundAttenuation> DefaultAttenuation;
};