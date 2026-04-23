// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/MyCameraActor.h"
#include "Camera/CameraComponent.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Kismet/GameplayStatics.h"

// Constructor
AMyCameraActor::AMyCameraActor()
{
	//Create audio component
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);

	// Disable aspect-ratio constraint to prevent letterbox / pillarbox black bars.
	// When true this forces a fixed aspect ratio and pads the viewport with black bars
	// whenever the window ratio doesn't match. The game world camera leaves this false,
	// so the login camera must do the same.
	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->bConstrainAspectRatio = false;
	}
}

// Play sound
void AMyCameraActor::PlaySound(USoundBase* Sound)
{
	// Route through the Music SoundClass so AudioManager volume sliders work
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(this)))
	{
		if (AudioComponent && GI->AudioManager && GI->AudioManager->MusicClass)
		{
			AudioComponent->SoundClassOverride = GI->AudioManager->MusicClass;
		}
	}

	AudioComponent->SetSound(Sound);
	AudioComponent->Play();
}

// Stop sound
void AMyCameraActor::StopSound()
{
	AudioComponent->Stop();
}