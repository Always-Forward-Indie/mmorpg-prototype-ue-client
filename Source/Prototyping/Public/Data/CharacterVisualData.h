// Character Visual Data — DataTable row that maps class+race+gender to visual assets.
// Row Name format: "classSlug_raceSlug_genderName" (e.g. "warrior_human_male")

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CharacterVisualData.generated.h"

class UNiagaraSystem;

// ─────────────────────────────────────────────────────────────────────────────
// Visual payload — reusable for both DataTable rows and runtime lookups
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FCharacterVisualData
{
	GENERATED_BODY()

	/** Skeletal mesh to use for the character body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Animation Blueprint class to drive the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TSoftClassPtr<UAnimInstance> AnimBPClass;

	/** Actor scale override (default 1,1,1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FVector ActorScale = FVector(1.0f);

	/** Height offset for floating combat text / hit effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float CombatHitHeight = 120.0f;

	/** Portrait icon shown in the character select list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSoftObjectPtr<UTexture2D> PortraitIcon;

	/** Audio profile ID passed to the entity audio system (e.g. "warrior_m"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	FName AudioProfileId = NAME_None;

	/** Optional death VFX. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TSoftObjectPtr<UNiagaraSystem> DeathVFX;
};

// ─────────────────────────────────────────────────────────────────────────────
// DataTable row — one per class+race+gender combination
// Row Name = "classSlug_raceSlug_genderName" (e.g. "mage_human_male")
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FCharacterVisualDefinition : public FTableRowBase
{
	GENERATED_BODY()

	/** Class slug matching server creation options (e.g. "warrior"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identification")
	FString ClassSlug;

	/** Race slug matching server creation options (e.g. "human"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identification")
	FString RaceSlug;

	/** Gender name matching server creation options (e.g. "male"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identification")
	FString GenderName;

	/** Visual assets for this combination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FCharacterVisualData Visual;

	// ─── Default Cosmetics ───────────────────────────────────────────────────
	// Row name keys into CharacterCosmeticsDataTable (UMyGameInstance::CharacterCosmeticsDataTable).
	// Leave NAME_None for races/classes that have no hair (e.g. helmet-only look, robots, etc.).

	/** Default hair cosmetic slug for this class+race+gender combination.
	 *  Example: "hair_human_female_01" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Cosmetics")
	FName DefaultHairSlug = NAME_None;

	/** Default facial hair cosmetic slug (beard, moustache, etc.).
	 *  Example: "beard_human_male_01" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Cosmetics")
	FName DefaultFacialHairSlug = NAME_None;
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper — builds the DataTable row name key
// ─────────────────────────────────────────────────────────────────────────────

namespace CharacterVisualHelper
{
	/** Build composite key from individual strings (lowercased). */
	inline FName MakeRowKey(const FString& ClassSlug, const FString& RaceSlug, const FString& GenderName)
	{
		return FName(*FString::Printf(TEXT("%s_%s_%s"),
			*ClassSlug.ToLower(), *RaceSlug.ToLower(), *GenderName.ToLower()));
	}

	/**
	 * Look up a visual definition with a fallback chain:
	 *   1) classSlug_raceSlug_genderName  (exact)
	 *   2) classSlug_raceSlug_male        (gender fallback)
	 *   3) classSlug_human_male           (race+gender fallback)
	 * Returns nullptr if nothing found.
	 */
	inline const FCharacterVisualDefinition* FindVisualDefinition(
		const UDataTable* Table,
		const FString& ClassSlug,
		const FString& RaceSlug,
		const FString& GenderName)
	{
		if (!Table) return nullptr;

		FName Key;

		// If gender is empty, try female first before falling back to male
		const FString EffectiveGender = GenderName.IsEmpty() ? TEXT("female") : GenderName;
		Key = MakeRowKey(ClassSlug, RaceSlug, EffectiveGender);
		if (const FCharacterVisualDefinition* Found = Table->FindRow<FCharacterVisualDefinition>(Key, TEXT("CharVisual")))
			return Found;

		// Try male if female not found OR if the original gender was explicitly not empty and not male
		if (!EffectiveGender.Equals(TEXT("male"), ESearchCase::IgnoreCase))
		{
			Key = MakeRowKey(ClassSlug, RaceSlug, TEXT("male"));
			if (const FCharacterVisualDefinition* Found = Table->FindRow<FCharacterVisualDefinition>(Key, TEXT("CharVisual")))
				return Found;
		}

		// Fallback: race → human, gender → male
		Key = MakeRowKey(ClassSlug, TEXT("human"), TEXT("male"));
		return Table->FindRow<FCharacterVisualDefinition>(Key, TEXT("CharVisual"));
	}
}
