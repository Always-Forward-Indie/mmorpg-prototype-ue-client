// Character Cosmetic Data — DataTable row that describes one cosmetic asset variant.
// Row Name = cosmetic slug, e.g. "hair_human_female_01", "beard_human_male_01".
//
// Cosmetic mesh type priority: StaticMesh > SkeletalMesh.
//   - Use SkeletalMesh when the cosmetic needs to animate with the body (hair, beard).
//     It must share the same Skeleton asset — SetLeaderPoseComponent will be called.
//   - Use StaticMesh for rigid cosmetics that don't need skinning (horns, crowns,
//     flower accessories, etc.). These are attached to a named socket instead.
//
// Assign the DataTable to UMyGameInstance::CharacterCosmeticsDataTable.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CharacterCosmeticData.generated.h"

/**
 * One row in the CharacterCosmeticsDataTable.
 *
 * Example rows:
 *   "hair_human_female_01"  → CosmeticSlot="hair",        SkeletalMesh=..., HideWhenEquipSlotsOccupied=["head"]
 *   "crown_01"              → CosmeticSlot="head_deco",   StaticMesh=...,   AttachSocketName="head_socket"
 *   "beard_human_male_01"   → CosmeticSlot="facial_hair", SkeletalMesh=..., HideWhenEquipSlotsOccupied=["head","chest"]
 *
 * Mesh priority at runtime: StaticMesh is used when set; SkeletalMesh is the fallback.
 */
USTRUCT(BlueprintType)
struct FCharacterCosmeticData : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * Logical cosmetic slot this entry belongs to.
	 * Supported values: "hair", "facial_hair", "eyebrows", "head_deco", etc.
	 * Only one active cosmetic per slot is allowed at a time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic")
	FName CosmeticSlot = NAME_None;

	/**
	 * Static mesh variant (rigid cosmetics: crowns, horns, flower clips, etc.).
	 * Takes priority over SkeletalMesh when both are set.
	 * Attached to AttachSocketName on the character skeleton.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic|Mesh")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	/**
	 * Socket name on the character skeleton to attach a StaticMesh cosmetic.
	 * Ignored when SkeletalMesh is used (Leader Pose does not use a socket).
	 * Example: "head_socket", "hat_socket".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic|Mesh")
	FName AttachSocketName = NAME_None;

	/**
	 * Optional relative transform applied to a StaticMesh cosmetic after socket attachment.
	 * Use to fine-tune position / rotation / scale per cosmetic row.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic|Mesh")
	FTransform StaticMeshRelativeTransform;

	/**
	 * Skeletal mesh variant (animated cosmetics: hair, beard, eyebrows, etc.).
	 * Must share the same Skeleton asset as the character body mesh so that
	 * SetLeaderPoseComponent works correctly.
	 * Used only when StaticMesh is null.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic|Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/**
	 * Equipment slot slugs whose occupation causes this cosmetic mesh to be hidden.
	 *
	 * Examples:
	 *   {"head"}          — any helmet hides the hair.
	 *   {"head","chest"}  — great-helm covering the neck also hides facial hair.
	 *
	 * Leave empty if this cosmetic should always be visible regardless of equipment.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic")
	TArray<FName> HideWhenEquipSlotsOccupied;
};
