// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AmbientCreatureDefinition.generated.h"

/**
 * UAmbientCreatureDefinition
 *
 * Data Asset describing the visual and audio identity of one ambient creature type.
 * Create one per species (e.g. DA_Def_Deer, DA_Def_Rabbit, DA_Def_Bird).
 * Assign to AAmbientCreatureActor::CreatureDefinition in the Details Panel —
 * the mesh and AnimBP will update live in the Viewport via OnConstruction.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UAmbientCreatureDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// ── Visual ───────────────────────────────────────────────────────────────

	/** Skeletal mesh for this creature. Applied immediately in the editor viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	/** Animation Blueprint class to drive the skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSubclassOf<UAnimInstance> AnimBPClass = nullptr;

	/** Relative offset applied to the mesh component (Z offset for ground alignment). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector MeshRelativeOffset = FVector(0.f, 0.f, -90.f);

	/** Scale applied to the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector ActorScale = FVector(1.f, 1.f, 1.f);

	// ── Idle Animations ───────────────────────────────────────────────────────

	/** Default looping idle montage — plays first and loops until a variant triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations|Idle")
	TObjectPtr<UAnimMontage> DefaultIdleMontage = nullptr;

	/** Pool of idle variant montages — one is picked randomly on a timer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations|Idle")
	TArray<TObjectPtr<UAnimMontage>> IdleVariantMontages;

	/** Minimum seconds between random idle variant plays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations|Idle", meta = (ClampMin = "1.0"))
	float IdleVariantMinDelay = 8.f;

	/** Maximum seconds between random idle variant plays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations|Idle", meta = (ClampMin = "1.0"))
	float IdleVariantMaxDelay = 20.f;

	// ── Audio ─────────────────────────────────────────────────────────────────

	/** Random idle sounds played on a timer while the creature stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> IdleSounds;

	/** Minimum seconds between random idle sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "5.0"))
	float IdleSoundMinDelay = 10.f;

	/** Maximum seconds between random idle sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "5.0"))
	float IdleSoundMaxDelay = 30.f;

	// ── Collision ─────────────────────────────────────────────────────────────

	/**
	 * Capsule half-height (from center to top/bottom). Controls how tall the creature
	 * stands — also used as NavMesh AgentHeight (x2) for pathfinding.
	 * Deer ≈ 88, Rabbit ≈ 22, Bird ≈ 15.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "1.0"))
	float CapsuleHalfHeight = 88.f;

	/**
	 * Capsule radius — physical collision size of the creature.
	 * Код автоматически выберет NavMesh агента с наименьшим AgentRadius >= CapsuleRadius
	 * из Project Settings → Navigation System → Supported Agents.
	 * Deer ≈ 30, Rabbit ≈ 15, Bird ≈ 12.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "1.0"))
	float CapsuleRadius = 30.f;

	// ── Movement ─────────────────────────────────────────────────────────────

	/** Walk speed used by CharacterMovementComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 150.f;

	/** Rotation rate while moving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float RotationRate = 270.f;
};
