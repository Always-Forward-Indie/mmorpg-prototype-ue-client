// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Data/DataStructs.h"
#include "MOBAnimInstance.generated.h"

/**
 * C++ AnimInstance for BasicMOB.
 * Stores all animation state variables in one place.
 * Anim BP inherits from this class and reads variables directly — no casting needed every tick.
 *
 * HOW PLAYRATE WORKS:
 *   combatInitiation sends animationDuration = how long the attack should take (seconds).
 *   We calculate PlayRate = Montage.SequenceLength / animationDuration so the montage
 *   finishes exactly when the server expects the hit to land.
 */
UCLASS()
class PROTOTYPING_API UMOBAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UMOBAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	//////////////////////////////////////////////////////////////////////////
	// Movement
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Movement")
	float Speed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Movement")
	bool bIsMoving = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Movement")
	bool bIsFleeing = false;

	//////////////////////////////////////////////////////////////////////////
	// Combat state
	//////////////////////////////////////////////////////////////////////////

	// True while an attack montage is playing
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Combat")
	bool bIsAttacking = false;

	// True when mob has an active target
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Combat")
	bool bIsAggressive = false;

	// Latched true on death — never reset
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Combat")
	bool bIsDead = false;

	// True for ~0.15 s after receiving a hit (drives hit-react state)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Combat")
	bool bIsHit = false;

	// Slot name sent to the AnimGraph Slot node (e.g. "attack_slash")
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Combat")
	FName CurrentAttackSlot = NAME_None;

	// Actual play-rate used for the current montage (visible for debugging)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MOB|Combat")
	float CurrentAttackPlayRate = 1.0f;

	//////////////////////////////////////////////////////////////////////////
	// Montage map — assign in Blueprint defaults or via code
	// Key   = animationName string from server (e.g. "attack_slash")
	// Value = UAnimMontage asset to play
	//////////////////////////////////////////////////////////////////////////
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MOB|Montages")
	TMap<FName, UAnimMontage*> AttackMontageMap;

	// Fallback montage played when AnimationName has no entry in the map
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MOB|Montages")
	UAnimMontage* DefaultAttackMontage = nullptr;

	// Montage played on hit-react (plays via Slot "HitReact" over the State Machine)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MOB|Montages")
	UAnimMontage* HitReactMontage = nullptr;
	// NOTE: Death is handled by the State Machine (A_Death Sequence, Loop=false)
	// so no DeathMontage is needed here.

	//////////////////////////////////////////////////////////////////////////
	// Called by BasicMOB event handlers
	//////////////////////////////////////////////////////////////////////////

	// Play attack montage scaled to animationDuration from combatInitiation
	void StartAttack(const FSkillInitiationData& SkillData);

	void NotifyDeath();
	void NotifyTargetLost();
	void NotifyHit();

	// Delegate fired when the hit-point frame is reached during an attack montage.
	// Bound by BasicMOB to forward the event to CombatSystemManager::NotifyHitPoint.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHitPoint, int32 /*CasterId*/);
	FOnHitPoint OnHitPoint;

	// Called by UAnimNotify_HitPoint (placed on the montage timeline) or by the
	// fallback timer when no notify is present in the montage asset.
	void FireHitPoint();

private:
TWeakObjectPtr<class ABasicMOB> OwnerMOB;

FOnMontageEnded MontageEndedDelegate;

	// CasterId of the currently playing attack (set in StartAttack, cleared on montage end)
	int32 CurrentCasterId = 0;

	// Timer handle for the hit-point notify (fallback when no AN_HitPoint in montage)
	FTimerHandle HitPointTimerHandle;

	// Ratio of animationDuration at which the hit frame occurs (0..1).
	// Driven by the AN_HitPoint Animation Notify position in the montage.
	// If no notify is present we fall back to this default ratio.
	static constexpr float DefaultHitRatio = 0.45f;

// Resolve montage asset from animationName, returns nullptr if nothing found
UAnimMontage* ResolveMontage(const FName& AnimationName) const;

// Calculate play rate so montage finishes in exactly DesiredDuration seconds
static float CalcPlayRate(const UAnimMontage* Montage, float DesiredDuration);

// Return the wall-clock delay (seconds) at which the hit frame occurs.
// Reads the position of the first notify named "HitPoint" or "Attack_Hit" on
// the montage timeline and converts it using the clamped PlayRate.
// Falls back to DefaultHitRatio * ActualDuration when no such notify exists.
static float CalcHitDelay(const UAnimMontage* Montage, float PlayRate, float ActualDuration);

UFUNCTION()
void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
