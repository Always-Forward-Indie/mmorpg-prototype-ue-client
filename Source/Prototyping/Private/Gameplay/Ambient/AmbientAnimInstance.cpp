// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/Ambient/AmbientAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UAmbientAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
}

void UAmbientAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter.IsValid()) return;

	const UCharacterMovementComponent* CMC = OwnerCharacter->GetCharacterMovement();
	if (!CMC) return;

	Speed     = CMC->Velocity.Size2D();
	bIsMoving = Speed > 20.f;
	bIsFlying = CMC->MovementMode == MOVE_Flying;
}
