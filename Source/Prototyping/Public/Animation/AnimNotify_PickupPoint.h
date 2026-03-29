#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PickupPoint.generated.h"

/**
 * Animation Notify that fires at the exact pickup frame of a pickup montage.
 * Place this notify on the montage timeline at the frame where the character's
 * hand visually reaches the item on the ground.
 *
 * When triggered it calls UPlayerAnimInstance::FirePickupPoint(), which
 * broadcasts OnPickupPoint so that ItemManager can destroy the DroppedItemActor
 * at the precise moment of visual contact.
 */
UCLASS(meta = (DisplayName = "Pickup Point"))
class PROTOTYPING_API UAnimNotify_PickupPoint : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("Pickup Point");
	}
};
