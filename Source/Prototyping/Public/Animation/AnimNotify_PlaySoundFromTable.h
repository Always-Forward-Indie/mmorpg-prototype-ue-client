#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlaySoundFromTable.generated.h"

/**
 * Animation Notify that plays a random sound from the mob's audio data table
 * based on a named sound slot.
 *
 * This allows ONE animation asset (e.g. "Idle_Yawn") to be shared across
 * multiple mob types, with each mob playing its OWN sound. The actual sound
 * is resolved at runtime from FMobAudioData via the mob's DataTable row.
 *
 * HOW TO USE:
 *   1. Open any shared animation in the Animation Editor.
 *   2. Add Notify ? Play Sound From Table at the desired frame.
 *   3. Set SoundSlotName to one of:
 *        "Idle"        → picks from FMobAudioData::IdleSounds[]
 *        "Walk"        → picks from FMobAudioData::WalkSounds[]
 *        "Run"         → picks from FMobAudioData::RunSounds[]
 *        "Voice"       → picks random from FMobAudioData::AttackVoiceSounds[] (melee swing cry)
 *        "CastVoice"   → picks random from FMobAudioData::CastVoiceSounds[]  (cast-start voice)
 *        "ReleaseVoice"→ picks random from FMobAudioData::ReleaseVoiceSounds[] (cast-release shout)
 *        "Swing"       → uses FMobAudioData::SwingSound (mob-specific weapon/limb whoosh)
 *        "Attack"      → uses FMobAudioData::AttackSound
 *        "Hit"         → uses FMobAudioData::HitSound
 *        "Death"       → uses FMobAudioData::DeathSound
 *        "Aggro"       → uses FMobAudioData::AggroSound
 *
 * Works on any ABasicMOB. Silently ignored on players or non-mob actors.
 */
UCLASS(meta = (DisplayName = "Play Sound From Table"))
class PROTOTYPING_API UAnimNotify_PlaySoundFromTable : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_PlaySoundFromTable();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return FString::Printf(TEXT("SoundTable: %s"), *SoundSlotName.ToString());
	}

	/**
	 * Which sound slot to resolve from the mob's audio data.
	 * "Idle", "Walk", "Run", "Attack", "Hit", "Death", "Aggro".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound From Table")
	FName SoundSlotName = TEXT("Idle");

	/**
	 * Foot socket used as the surface-trace origin for Walk / Run slots.
	 * Set "foot_l" on left-foot notifies and "foot_r" on right-foot notifies.
	 * Falls back to the actor root location when the socket is not found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound From Table")
	FName FootSocketName = TEXT("foot_l");

	/** Volume multiplier for this notify instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound From Table",
	          meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeMultiplier = 1.0f;
};
