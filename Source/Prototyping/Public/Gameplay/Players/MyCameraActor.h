// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Components/AudioComponent.h"
#include "MyCameraActor.generated.h"

/**
 * 
 */
UCLASS()
class PROTOTYPING_API AMyCameraActor : public ACameraActor
{
	GENERATED_BODY()
	

public:
	AMyCameraActor();

	// Audio component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	UAudioComponent* AudioComponent;

	// Play sound
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlaySound(USoundBase* Sound);

	// Stop sound
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void StopSound();
};
