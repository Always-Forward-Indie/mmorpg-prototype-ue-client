#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Engine/DataTable.h"
#include "NiagaraSystem.h"
#include "AnimNotify_Footstep.generated.h"

/** One row in the FootstepSoundsTable. RowName = Physical Material name. */
USTRUCT(BlueprintType)
struct FFootstepSoundData : public FTableRowBase
{
	GENERATED_BODY()

	/** Pool of sounds to pick from at random. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<TSoftObjectPtr<USoundBase>> FootstepSounds;

	/** Optional Niagara particle effect (dust, splash, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TSoftObjectPtr<UNiagaraSystem> FootstepVFX;

	/** Per-surface volume multiplier (stacked with the notify's own multiplier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep",
	          meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeMultiplier = 1.0f;

	/** Per-surface attenuation override. When set, takes priority over the entity's
	 *  DefaultAttenuation from its audio profile. E.g. grass=15m, stone=30m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TSoftObjectPtr<USoundAttenuation> DefaultAttenuation;

	FFootstepSoundData() {}
};

/**
 * Animation Notify that plays a footstep sound based on the physical material
 * under the character's foot at the moment the notify fires.
 *
 * HOW TO USE:
 *   1. Open any Walk/Run animation in the Animation Editor.
 *   2. On the Notifies track, right-click ? Add Notify ? Footstep.
 *   3. Place on the exact frame when the foot touches the ground.
 *   4. In Details, choose which foot (Left / Right) for correct trace origin.
 *
 * DATA SETUP:
 *   - Create a DataTable with row struct = FFootstepSoundData.
 *   - RowName = Physical Material name (e.g. "PM_Grass", "PM_Stone").
 *   - Assign the DataTable in GameInstance BP ? FootstepSoundsTable.
 *
 * The notify traces down from the foot socket, reads the Physical Material,
 * looks up the DataTable row, and plays a random sound from that row.
 */
UCLASS(meta = (DisplayName = "Footstep"))
class PROTOTYPING_API UAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_Footstep();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("Footstep");
	}

	/** Which foot this notify is for � determines the line-trace origin socket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	FName FootSocketName = TEXT("foot_l");

	/** Volume multiplier for this specific notify instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep",
	          meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeMultiplier = 1.0f;

	/** Fallback sound if no Physical Material match is found in the DataTable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TSoftObjectPtr<USoundBase> DefaultFootstepSound;
};
