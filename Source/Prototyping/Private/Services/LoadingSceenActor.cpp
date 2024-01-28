// Fill out your copyright notice in the Description page of Project Settings.


#include "Services/LoadingSceenActor.h"

// Sets default values
ALoadingSceenActor::ALoadingSceenActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Initialize root component
	USceneComponent* MyRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MyRootComponent"));
	RootComponent = MyRootComponent;

	//Create audio component
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ALoadingSceenActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ALoadingSceenActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Play sound
void ALoadingSceenActor::PlaySound(USoundBase* Sound)
{
	AudioComponent->SetSound(Sound);
	AudioComponent->Play();
}

// Stop sound
void ALoadingSceenActor::StopSound()
{
	AudioComponent->Stop();
}
