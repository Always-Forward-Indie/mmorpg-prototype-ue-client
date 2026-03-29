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
 * when they leave.  Multiple zones can overlap — each plays independently.
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

	/** Trigger volume — resize to define the zone boundary. */
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

	/** Volume multiplier applied to this zone's sound (0–1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient Zone",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VolumeMultiplier = 1.0f;

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

private:
	UPROPERTY()
	UAudioComponent* AudioComponent = nullptr;

	FTimerHandle OverlapCheckTimerHandle;

	void CheckInitialOverlap();

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
