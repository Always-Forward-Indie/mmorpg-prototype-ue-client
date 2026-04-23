// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AmbientAnimInstance.generated.h"

/**
 * UAmbientAnimInstance
 *
 * Base AnimInstance for all ambient creatures.
 * Automatically updates Speed, bIsMoving and bIsFlying every frame
 * by reading from the owning Character's CharacterMovementComponent.
 *
 * Designer workflow:
 *   1. Create ABP_Deer (Animation Blueprint) — set Parent Class to UAmbientAnimInstance.
 *   2. In the AnimGraph, create a State Machine with at least two states:
 *        - Idle  : plays a looping idle animation sequence
 *        - Walk  : plays a looping walk BlendSpace or animation sequence
 *   3. Add a transition Idle → Walk:  "Speed > 20"
 *      Add a transition Walk → Idle:  "Speed < 20"
 *   4. Add a "DefaultSlot" node after the State Machine output to allow
 *      montages (PlayMontage behaviors) to overlay on top.
 *      Chain:  [State Machine] → [DefaultSlot] → [Output Pose]
 *   5. Assign this AnimBP in DA_Def_Deer → Anim BP Class.
 *
 * For flying creatures (FlyAlong behavior):
 *   - Add a Fly state driven by bIsFlying.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UAmbientAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ── State variables — read these in the AnimBP State Machine ─────────────

	/**
	 * Current horizontal movement speed (cm/s).
	 * Use as a transition condition: Idle→Walk when Speed > 20, Walk→Idle when Speed < 20.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient|Movement")
	float Speed = 0.f;

	/**
	 * True when the creature is actively moving (Speed > threshold).
	 * Alternative to Speed for simple two-state machines.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient|Movement")
	bool bIsMoving = false;

	/**
	 * True when the creature's movement mode is Flying (set by FlyAlong behavior).
	 * Use to blend into a flight pose in the State Machine.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient|Movement")
	bool bIsFlying = false;

private:
	TWeakObjectPtr<ACharacter> OwnerCharacter;
};
