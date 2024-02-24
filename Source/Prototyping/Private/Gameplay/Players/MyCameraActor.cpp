// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/MyCameraActor.h"

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
	AudioComponent->SetSound(Sound);
	AudioComponent->Play();
}

// Stop sound
void AMyCameraActor::StopSound()
{
	AudioComponent->Stop();
}