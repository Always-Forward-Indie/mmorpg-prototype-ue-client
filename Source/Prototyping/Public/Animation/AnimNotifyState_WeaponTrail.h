#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_WeaponTrail.generated.h"

class UNiagaraComponent;

/**
 * Animation Notify State that activates a weapon trail VFX during a swing.
 *
 * HOW TO USE:
 *   1. Open the attack montage in the Animation Editor.
 *   2. On the Notifies track, right-click ? Add Notify State ? Weapon Trail.
 *   3. Drag the begin/end handles to cover the swing arc frames.
 *
 * The notify reads EquippedSwingVFX from FItemVisualData (via EquipmentVisualComponent)
 * for the item currently in the weapon slot (default: "main_hand").
 * If the item has WeaponTrailTipSocket / WeaponTrailBaseSocket defined,
 * they are passed as Niagara User Parameters so the ribbon follows the blade.
 *
 * Works on any character that has a UEquipmentVisualComponent.
 */
UCLASS(meta = (DisplayName = "Weapon Trail"))
class PROTOTYPING_API UAnimNotifyState_WeaponTrail : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_WeaponTrail();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return TEXT("Weapon Trail");
	}

	/** Equipment slot slug to read weapon visual data from (default "main_hand"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Trail")
	FString WeaponSlotSlug = TEXT("main_hand");

private:
	UPROPERTY()
	TWeakObjectPtr<UNiagaraComponent> ActiveTrailComponent;
};
