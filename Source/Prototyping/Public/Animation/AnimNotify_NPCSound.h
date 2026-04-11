#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_NPCSound.generated.h"

/**
 * Animation Notify that plays a sound on an NPC at a specific frame of a montage.
 *
 * The sound is resolved from ABasicNPC::SoundMap by slot name and played through
 * AudioComponentMain, which is already routed to the SFX SoundClass (volume slider works).
 *
 * Built-in slot names:
 *   "Greeting"  – plays the NPC's greeting voice line
 *   "Farewell"  – plays the NPC's farewell voice line
 *   "Interact"  – plays the NPC's interaction click sound
 *   "Idle"      – plays a random sound from IdleSounds[]
 *
 * HOW TO USE:
 *   1. Open any NPC montage (e.g. AM_NPC_Greet) in the Animation Editor.
 *   2. In the Notifies track add → NPC Sound Slot at the desired frame.
 *   3. Set SoundSlotName to one of the keys above.
 *
 * This lets you trigger the greeting voice at the exact frame the NPC turns its head,
 * instead of always at montage start.
 */
UCLASS(meta = (DisplayName = "NPC Sound Slot"))
class PROTOTYPING_API UAnimNotify_NPCSound : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return FString::Printf(TEXT("NPCSound: %s"), *SoundSlotName.ToString());
	}

	/**
	 * Which sound slot to resolve from ABasicNPC::SoundMap.
	 * Use "Idle" to trigger a random idle voice line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Sound")
	FName SoundSlotName = TEXT("Greeting");
};
