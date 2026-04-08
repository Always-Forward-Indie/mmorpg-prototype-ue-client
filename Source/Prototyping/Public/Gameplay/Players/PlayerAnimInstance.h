// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Data/DataStructs.h"
#include "PlayerAnimInstance.generated.h"

class ABasicPlayer;

/**
 * C++ AnimInstance for BasicPlayer.
 * Mirrors UMOBAnimInstance: all animation state lives here so the Anim BP
 * can read variables directly without casting.
 *
 * HOW PLAYRATE WORKS:
 *   combatInitiation sends animationDuration = how long the attack should take (seconds).
 *   We calculate PlayRate = Montage.SequenceLength / animationDuration so the montage
 *   finishes exactly when the server expects the hit to land.
 *
 * PACKET FLOW:
 *   combatInitiation  --> BasicPlayer::PlaySkillAnimation_Implementation --> StartAttack()
 *   combatResult      --> (hit-point timer or AN_HitPoint notify) --> FireHitPoint()
 *                     --> BasicPlayer::ShowDamageEffect_Implementation   --> NotifyHit()
 *   death             --> BasicPlayer::OnDeath_Implementation            --> NotifyDeath()
 */
UCLASS()
class PROTOTYPING_API UPlayerAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UPlayerAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    //////////////////////////////////////////////////////////////////////////
    // Movement
    //////////////////////////////////////////////////////////////////////////

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Movement")
    float Speed = 0.0f;

    // MaxWalkSpeed mirrored from CMC each tick so the Anim BP blend space can
    // normalise Speed without hard-coding a speed constant in the graph.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Movement")
    float MaxSpeed = 200.0f;

    // Speed normalised to [0..1] relative to MaxSpeed.
    // Use this as the Blend Space axis input instead of raw Speed so that
    // the walk/run cycle rate always matches the actual movement speed,
    // regardless of what value the server sends for move_speed.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Movement")
    float SpeedNormalized = 0.0f;

    // Movement direction angle in degrees relative to the actor's forward vector.
    // Range: -180..180. 0 = forward, -90 = left, +90 = right, �180 = backward.
    // Use as the horizontal axis of a Blend Space 2D alongside SpeedNormalized.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Movement")
    float Direction = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Movement")
    bool bIsMoving = false;

    //////////////////////////////////////////////////////////////////////////
    // Combat state
    //////////////////////////////////////////////////////////////////////////

    // True while an attack montage is playing
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat")
    bool bIsAttacking = false;

    // True when player has an active target
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat")
    bool bIsAggressive = false;

    // Latched true on death � never reset
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat")
    bool bIsDead = false;

    // True for ~0.15 s after receiving a hit (drives hit-react state)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat")
    bool bIsHit = false;

    // Slot name sent to the AnimGraph Slot node (e.g. "attack_slash")
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat")
    FName CurrentAttackSlot = NAME_None;

    // Actual play-rate used for the current montage (visible for debugging)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat")
    float CurrentAttackPlayRate = 1.0f;

    //////////////////////////////////////////////////////////////////////////
    // Montage map � assign in Blueprint defaults or via code
    // Key   = animationName string from server (e.g. "attack_slash")
    // Value = UAnimMontage asset to play
    //////////////////////////////////////////////////////////////////////////
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player|Montages")
    TMap<FName, UAnimMontage*> AttackMontageMap;

    // Fallback montage played when AnimationName has no entry in the map
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player|Montages")
    UAnimMontage* DefaultAttackMontage = nullptr;

    // Montage played on hit-react (plays via Slot "HitReact" over the State Machine)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player|Montages")
    UAnimMontage* HitReactMontage = nullptr;
    // NOTE: Death is handled by the State Machine (A_Death Sequence, Loop=false)
    // so no DeathMontage is needed here.

    // Montage played when the player picks up an item from the ground.
    // Assign a short "reach down" animation in the AnimBP defaults.
    // If null, the pickup request is sent immediately with no animation.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Player|Montages")
    UAnimMontage* PickupMontage = nullptr;

    //////////////////////////////////////////////////////////////////////////
    // Called by BasicPlayer event handlers
    //////////////////////////////////////////////////////////////////////////

    // Play attack montage scaled to animationDuration from combatInitiation
    void StartAttack(const FSkillInitiationData& SkillData);

    void NotifyDeath();
    void NotifyRevive();
    void NotifyTargetLost();
    void NotifyHit();

    // Plays PickupMontage and returns its duration in seconds (0 if no montage assigned).
    float NotifyPickup();

    // Delegate fired when the hit-point frame is reached during an attack montage.
    // Bound by BasicPlayer to forward the event to CombatSystemManager::NotifyHitPoint.
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnHitPoint, int32 /*CasterId*/);
    FOnHitPoint OnHitPoint;

    // Delegate fired when the attack montage fully ends (or is interrupted).
    // Used by PlayerSkillManager to unlock bIsCasting after the animation completes.
    DECLARE_MULTICAST_DELEGATE(FOnAttackEnded);
    FOnAttackEnded OnAttackEnded;

    // Delegate fired when the pickup-point frame is reached during a pickup montage.
    // Bind this in ItemManager to destroy the DroppedItemActor at the visual moment
    // the character's hand reaches the item. Falls back to end-of-montage if no
    // AnimNotify_PickupPoint is placed on the montage timeline.
    DECLARE_MULTICAST_DELEGATE(FOnPickupPoint);
    FOnPickupPoint OnPickupPoint;

    // Called by UAnimNotify_PickupPoint (placed on the montage timeline) or by the
    // fallback timer when no notify is present in the montage asset.
    void FirePickupPoint();

    // Cancels the fallback pickup-point timer without broadcasting OnPickupPoint.
    // Call this from ItemManager::ResetPickupState() so that a stale timer from
    // a previous pickup attempt never fires into a new one.
    void CancelPickupTimer();

    // Called by UAnimNotify_HitPoint (placed on the montage timeline) or by the
    // fallback timer when no notify is present in the montage asset.
    void FireHitPoint();

private:
    TWeakObjectPtr<ABasicPlayer> OwnerPlayer;

    FOnMontageEnded MontageEndedDelegate;

    // CasterId of the currently playing attack (set in StartAttack, cleared on montage end)
    int32 CurrentCasterId = 0;

    // Timer handle for the hit-point notify (fallback when no AN_HitPoint in montage)
    FTimerHandle HitPointTimerHandle;

    // Timer handle for the pickup-point notify (fallback when no AN_PickupPoint in montage)
    FTimerHandle PickupPointTimerHandle;

    // Timer handle for programmatic JumpToSection("CastRelease") at T=castTime.
    // Used when castTime > 0 AND the montage has a "CastRelease" section.
    FTimerHandle CastReleaseTimerHandle;

    // Pointer to the active cast montage, kept for Montage_SetPlayRate after the jump
    TWeakObjectPtr<UAnimMontage> ActiveCastMontage;

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

    // Returns true if the montage contains a section named "CastRelease".
    static bool HasCastReleaseSection(const UAnimMontage* Montage);

    // Called by CastReleaseTimerHandle: jumps the active montage to "CastRelease"
    // section and switches to natural (1.0x) playback rate so the release animation
    // plays at the speed the animator intended.
    void OnCastReleaseTimer();

    UFUNCTION()
    void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
