// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/MyPlayerBasic.h"

// Sets default values
AMyPlayerBasic::AMyPlayerBasic()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMyPlayerBasic::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyPlayerBasic::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AMyPlayerBasic::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

