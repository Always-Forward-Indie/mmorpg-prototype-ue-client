// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmbientSoundZoneActor.generated.h"

class UBoxComponent;
class UAudioComponent;
class USoundClass;

/**
 * AAmbientSoundZoneActor
 *
 * Place this actor in any level to add a looping ambient sound to a zone.
 * The sound fades in when the local player enters the box and fades out
 * when they leave.  Multiple zones can overlap � each plays independently.
 *
 * Designer workflow:
 *   1. Drag BP_AmbientSoundZoneActor into the level.
 *   2. Scale the BoxComponent to the area you want covered.
 *   3. Assign AmbientSound (a looping SoundWave, SoundCue or MetaSound).
 *   4. Adjust FadeInTime / FadeOutTime / VolumeMultiplier as needed.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API AAmbientSoundZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AAmbientSoundZoneActor();

	/** Trigger volume � resize to define the zone boundary. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Zone")
	UBoxComponent* TriggerBox = nullptr;

	/** The looping ambient sound asset to play inside this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone")
	USoundBase* AmbientSound = nullptr;

	/** Seconds to fade in when the player enters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone", meta = (ClampMin = "0.0"))
	float FadeInTime = 1.5f;

	/** Seconds to fade out when the player leaves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone", meta = (ClampMin = "0.0"))
	float FadeOutTime = 2.0f;

	/** Volume multiplier applied to this zone's sound (0�1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VolumeMultiplier = 1.0f;
	/** Loop the sound when it finishes playing.
	 *  Enable for non-looping SoundWave assets that should repeat while the player is inside.
	 *  If the SoundWave already has Looping enabled on the asset it will loop on its own
	 *  regardless of this flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone")
	bool bLoop = true;

	/** If true, the sound attenuates with distance from this actor's origin (3D point source).
	 *  The two attenuation spheres become visible and editable below.
	 *  If false (default), the sound plays as 2D the moment the player enters the box —
	 *  correct for large zone ambients (forest wind, cave rumble, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone|Attenuation")
	bool bSpatialized = false;

	/** Inner radius: full-volume sphere around the actor. Only active when bSpatialized = true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone|Attenuation",
		meta = (ClampMin = "0.0", EditCondition = "bSpatialized", EditConditionHides))
	float AttenuationInnerRadius = 500.0f;

	/** Outer radius: sound is fully silent beyond this distance. Only active when bSpatialized = true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone|Attenuation",
		meta = (ClampMin = "0.0", EditCondition = "bSpatialized", EditConditionHides))
	float AttenuationOuterRadius = 2000.0f;
	/**
	 * If true, the sound starts playing immediately in BeginPlay without
	 * waiting for the player to enter the trigger box.
	 * Use this on levels that have no player pawn (e.g. login screen, loading screen).
	 * The overlap callbacks still work, but the initial play is unconditional.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone")
	bool bAutoPlay = false;
	/** SoundClass override for this zone's AudioComponent.
	 *  If not set, the zone will auto-assign the AmbientClass from AudioManager
	 *  so that the Ambient volume slider in settings controls these sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone")
	USoundClass* SoundClassOverride = nullptr;

	/** Play/stop the ambient sound from Blueprint if needed. */
	UFUNCTION(BlueprintCallable, Category = "Ambient Zone")
	void StartAmbient();

	UFUNCTION(BlueprintCallable, Category = "Ambient Zone")
	void StopAmbient();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY()
	UAudioComponent* AudioComponent = nullptr;

	FTimerHandle OverlapCheckTimerHandle;

	/** True while the local player is inside the trigger box.
	 *  Used by OnAmbientFinished to restart the sound when bLoop = true. */
	bool bPlayerIsInside = false;

	/** Tracks how many zones are currently covering each ambient sound.
	 *  When entering a zone with the same sound as a neighbor, we increment
	 *  the counter instead of restarting playback.  Only the last zone to
	 *  be left actually stops the audio — avoiding gaps and restarts. */
	static TMap<USoundBase*, int32> ActiveAmbientRefCount;

	/** The AudioComponent that was used to start playback for each sound.
	 *  May belong to a different zone than the one calling StopAmbient. */
	static TMap<USoundBase*, UAudioComponent*> ActiveAmbientComponents;

	/** Applies bSpatialized / AttenuationInnerRadius / AttenuationOuterRadius to
	 *  the AudioComponent. Called from both OnConstruction (editor preview) and
	 *  BeginPlay (runtime) so the viewport spheres stay in sync with Details edits. */
	void ApplyAttenuationSettings();

	void CheckInitialOverlap();

	/** Called by AudioComponent::OnAudioFinished. Restarts playback if bLoop and player is still inside. */
	UFUNCTION()
	void OnAmbientFinished();

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
