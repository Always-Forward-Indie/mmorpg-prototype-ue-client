#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CameraShake.generated.h"

/**
 * Animation Notify that triggers a camera shake on the local player.
 * Place on any attack montage at the frame where the hit impact occurs.
 * Only fires on the locally controlled pawn — remote players are unaffected.
 *
 * Intensity is configurable per notify instance directly in the montage editor:
 * select the notify on the timeline and adjust ShakeIntensity in Details.
 */
UCLASS(meta = (DisplayName = "Camera Shake"))
class PROTOTYPING_API UAnimNotify_CameraShake : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_CameraShake();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("Camera Shake");
	}

	/** Shake scale forwarded to PlayCombatCameraShake. 1.0 = full intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ShakeIntensity = 1.0f;
};
