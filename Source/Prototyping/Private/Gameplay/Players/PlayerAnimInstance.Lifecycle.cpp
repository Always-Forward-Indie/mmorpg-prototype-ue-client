// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

UPlayerAnimInstance::UPlayerAnimInstance()
{
}

void UPlayerAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    OwnerPlayer = Cast<ABasicPlayer>(TryGetPawnOwner());
    MontageEndedDelegate.BindUObject(this, &UPlayerAnimInstance::OnAttackMontageEnded);
}

void UPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // Re-validate every tick: owner can change on respawn
    if (!OwnerPlayer.IsValid())
    {
        OwnerPlayer = Cast<ABasicPlayer>(TryGetPawnOwner());
    }

    if (!OwnerPlayer.IsValid()) return;

    ABasicPlayer* Player = OwnerPlayer.Get();

    // For remote (other-client) players the CharacterMovementComponent velocity is always
    // zero because movement is driven by SetActorLocation() not CMC.
    // Use RemoteSpeed, which is derived from consecutive server position deltas.
    // For the local player CMC velocity is authoritative � use it directly.
	if (Player->GetIsOtherClient())
	{
		Speed     = Player->GetRemoteSpeed();      // already EMA-smoothed in BasicPlayer
		bIsMoving = Speed > 1.0f;
		bIsInAir  = Player->GetRemoteIsInAir();    // driven by isFalling from server

		// Mirror CMC MaxWalkSpeed so the blend space normalisation works the same
		// for remote players as for the local player.
		const UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();
		MaxSpeed = (MoveComp && MoveComp->MaxWalkSpeed > 0.0f) ? MoveComp->MaxWalkSpeed : MaxSpeed;

		// Direction is EMA-smoothed in BasicPlayer::UpdateRemotePlayerMovement.
		// We read it unconditionally — it keeps its last value during the speed
		// fade-out so the blend-space doesn't snap to 0 while slowing down.
		Direction = Player->GetRemoteDirection();
	}
	else
	{
		const UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();
		Speed     = MoveComp ? MoveComp->Velocity.Size2D() : 0.0f;
		bIsMoving = Speed > 1.0f;
		bIsInAir  = MoveComp ? MoveComp->IsFalling() : false;
		MaxSpeed  = (MoveComp && MoveComp->MaxWalkSpeed > 0.0f) ? MoveComp->MaxWalkSpeed : MaxSpeed;

        // Compute direction angle between actor forward and velocity vector.
        // 0 = forward, -90 = left, +90 = right, �180 = backward.
        if (bIsMoving && MoveComp)
        {
            const FVector VelDir      = MoveComp->Velocity.GetSafeNormal2D();
            const FVector ActorForward = Player->GetActorForwardVector();
            const FVector ActorRight   = Player->GetActorRightVector();
            Direction = FMath::RadiansToDegrees(
                FMath::Atan2(FVector::DotProduct(VelDir, ActorRight),
                             FVector::DotProduct(VelDir, ActorForward)));
        }
        else
        {
            Direction = 0.0f;
        }
    }

    // SpeedNormalized: 0.0 = standing still, 1.0 = moving at full MaxSpeed.
    // Wire this into the Blend Space horizontal axis so animation playback
    // stays in sync regardless of the server-assigned move_speed value.
    SpeedNormalized = (MaxSpeed > 0.0f) ? FMath::Clamp(Speed / MaxSpeed, 0.0f, 1.0f) : 0.0f;

    // Fallback sync � event handlers are the primary source for these
    bIsDead = Player->GetIsDead();
}

// ---------------------------------------------------------------------------
// StartAttack
//   Called from BasicPlayer::PlaySkillAnimation_Implementation when the server
//   sends combatInitiation and this player is the caster.
//   animationDuration = how many seconds the server expects the full attack to take.
//   We compute PlayRate = montage_length / animationDuration so the clip finishes
//   exactly when the server considers the hit resolved (combatResult arrives).
//   A hit-point timer fires at DefaultHitRatio * animationDuration so that FCT
//   is shown at the visual moment of impact rather than at the end of the clip.
