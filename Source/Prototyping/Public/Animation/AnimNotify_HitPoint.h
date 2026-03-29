#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_HitPoint.generated.h"

/**
 * Animation Notify that fires at the exact hit-frame of an attack montage.
 * Place this notify on the montage timeline at the frame where the weapon
 * visually connects with the target (e.g. wolf bite, sword swing impact).
 *
 * When triggered, it calls UMOBAnimInstance::FireHitPoint() which in turn
 * notifies CombatSystemManager to flush the cached combatResult — showing
 * FCT and applying HP changes exactly at the moment of visual impact.
 */
UCLASS(meta = (DisplayName = "Hit Point"))
class PROTOTYPING_API UAnimNotify_HitPoint : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("Hit Point");
	}
};
