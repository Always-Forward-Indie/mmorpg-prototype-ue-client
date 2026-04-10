// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NPCAnimInstance.generated.h"

/**
 * AnimInstance for BasicNPC.
 *
 * Blueprint setup:
 *   1. Create ABP_NPC (AnimBlueprint) inheriting from UNPCAnimInstance.
 *   2. In the State Machine read bIsMoving / Speed / bIsTalking etc.
 *   3. Add a "DefaultSlot" Slot node to play action montages (Greet, Farewell, Emote…).
 *   4. Assign montages in the Blueprint defaults (GreetMontage, FarewellMontage, ActionMontages map).
 *   5. Place UAnimNotify_PlayerCombatEvent (or a custom equivalent) on montages for audio cues.
 *
 * From BasicNPC C++ call the public trigger functions:
 *   GetNPCAnimInstance()->PlayAction(TEXT("idle_scratch")); // from a table key
 *   GetNPCAnimInstance()->NotifyGreet();
 *   GetNPCAnimInstance()->NotifyFarewell();
 *
 * Multiple idle animations:
 *   Populate IdleMontages[] in Blueprint defaults.  Call PickRandomIdleMontage() from
 *   a timer in BasicNPC (or from a Blueprint tick) to cycle idles.
 */
UCLASS()
class PROTOTYPING_API UNPCAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UNPCAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    //////////////////////////////////////////////////////////////////////////
    // Animation state — read in ABP_NPC State Machine
    //////////////////////////////////////////////////////////////////////////

    /** Current movement speed (cm/s). Driven by CharacterMovementComponent. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
    float Speed = 0.0f;

    /** True when the NPC is walking/running. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
    bool bIsMoving = false;

    /** True while a dialogue interaction is active (drives "talking" idle blend). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|State")
    bool bIsTalking = false;

    /**
     * Index of the currently active idle variant (0-based).
     * Bind this in the State Machine to select one of several idle sequences.
     * Updated automatically when PickRandomIdleMontage() or CycleIdleVariant() is called.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Idle")
    int32 IdleVariantIndex = 0;

    //////////////////////////////////////////////////////////////////////////
    // Montage assets — assign in Blueprint defaults
    //////////////////////////////////////////////////////////////////////////

    /**
     * Named action montages: key = action slug (e.g. "greet", "farewell", "laugh", "point").
     * Populate in Blueprint defaults.  PlayAction(Slug) picks the matching montage.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "NPC|Montages")
    TMap<FName, UAnimMontage*> ActionMontageMap;

    /**
     * Random idle variant montages played via PickRandomIdleMontage().
     * These play through the DefaultSlot so they blend over the locomotion SM.
     * Leave empty to use only the State-Machine idle sequence.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "NPC|Montages")
    TArray<UAnimMontage*> IdleMontages;

    //////////////////////////////////////////////////////////////////////////
    // Trigger functions — call from BasicNPC or Blueprint
    //////////////////////////////////////////////////////////////////////////

    /**
     * Play a named action montage (key from ActionMontageMap).
     * Returns false if no matching montage is found.
     */
    UFUNCTION(BlueprintCallable, Category = "NPC|Animation")
    bool PlayAction(FName ActionSlug);

    /**
     * Play the greeting montage ("greet" key) if assigned.
     * Fires the Blueprint event OnGreet for additional VFX / audio.
     */
    UFUNCTION(BlueprintCallable, Category = "NPC|Animation")
    void NotifyGreet();

    /**
     * Play the farewell montage ("farewell" key) if assigned.
     * Fires the Blueprint event OnFarewell.
     */
    UFUNCTION(BlueprintCallable, Category = "NPC|Animation")
    void NotifyFarewell();

    /**
     * Pick and play a random idle montage from IdleMontages[].
     * Updates IdleVariantIndex so the State Machine can reflect which idle is active.
     * No-op when IdleMontages is empty.
     */
    UFUNCTION(BlueprintCallable, Category = "NPC|Animation")
    void PickRandomIdleMontage();

    /**
     * Step IdleVariantIndex to the next slot (wraps around).
     * Use this from a Blueprint timer to cycle through idle variation states
     * that are driven purely by a State Machine (without montages).
     */
    UFUNCTION(BlueprintCallable, Category = "NPC|Animation")
    void CycleIdleVariant();

    /** Set bIsTalking (called when dialogue opens/closes). */
    UFUNCTION(BlueprintCallable, Category = "NPC|Animation")
    void SetTalking(bool bTalking);

    /** Stop any currently playing action montage (blend out). */
    UFUNCTION(BlueprintCallable, Category = "NPC|Animation")
    void StopCurrentAction(float BlendOutTime = 0.25f);

    //////////////////////////////////////////////////////////////////////////
    // Blueprint-implementable events — override in ABP_NPC for VFX/audio
    //////////////////////////////////////////////////////////////////////////

    /**
     * Fired when a "voice_greet" audio notifier position is hit inside a montage,
     * or when NotifyGreet() is called directly.  Override in Blueprint to play a
     * greeting voice line via AudioManager.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Events")
    void OnGreet();

    /**
     * Fired when a "voice_farewell" audio notifier position is hit inside a montage,
     * or when NotifyFarewell() is called directly.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Events")
    void OnFarewell();

    /**
     * Generic audio/VFX event fired from Animation Notifiers placed on any NPC montage.
     * Place a custom AnimNotify on the montage timeline and call
     * GetNPCAnimInstance()->OnAnimationEvent(FName("voice_idle")) from its Notify().
     * Override in Blueprint to resolve the event slug to an actual sound/effect.
     *
     * @param EventSlug  Identifies the event type, e.g. "voice_greet", "footstep", "emote_laugh".
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Events")
    void OnAnimationEvent(FName EventSlug);

private:
    TWeakObjectPtr<class ABasicNPC> OwnerNPC;

    /** Delegate used to clear bIsTalking / active action after a montage ends. */
    FOnMontageEnded ActionMontageEndedDelegate;

    /** Currently playing action montage (nullptr when idle). */
    UAnimMontage* ActiveActionMontage = nullptr;

    UFUNCTION()
    void OnActionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    /** Internal helper: play a montage and register the end delegate. */
    bool PlayMontageInternal(UAnimMontage* Montage, float PlayRate = 1.0f);
};
