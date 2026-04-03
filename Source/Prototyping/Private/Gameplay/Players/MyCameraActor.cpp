// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/MyCameraActor.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Kismet/GameplayStatics.h"

// Constructor
AMyCameraActor::AMyCameraActor()
{
	//Create audio component
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
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