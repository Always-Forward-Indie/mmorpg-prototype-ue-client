// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Animation/WidgetAnimation.h"
#include "Data/DataStructs.h"
#include "W_NPCNameplateWidget.generated.h"

/**
 * World-space nameplate widget for NPCs.
 *
 * Layout expected in the Blueprint (WBP_NPCNameplate):
 *
 *   RootScaleBox              (ScaleBox)
 *     NPCNameText             (TextBlock)  - NPC display name
 *     NPCTypeText             (TextBlock)  - e.g. "[Merchant]"          (BindWidgetOptional)
 *     NPCLevelText            (TextBlock)  - e.g. "Lv. 12"              (BindWidgetOptional)
 *     QuestIndicatorImage     (Image)      - yellow "!" quest icon       (BindWidgetOptional)
 *     QuestDoneImage          (Image)      - gold   "!" complete icon    (BindWidgetOptional)
 *     QuestInProgressImage    (Image)      - orange "!" in-progress icon (BindWidgetOptional)
 *     DialogueIndicatorImage  (Image)      - blue   "?" dialogue icon    (BindWidgetOptional)
 *     InteractHintText        (TextBlock)  - "Press F to talk"           (BindWidgetOptional)
 *
 * Call SetNPCInfo() once when the NPC actor is initialised.
 * Call SetPlayerInRange() every tick to show/hide the interact hint.
 * Call SetWidgetScale() / SetNameplateOpacity() from UNPCNameplateComponent.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UW_NPCNameplateWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ------------------------------------------------------------------ //
    //  Primary update - call once after NPC data is received from server  //
    // ------------------------------------------------------------------ //

    /**
     * Populate all nameplate fields.
     *
     * @param InName             NPC display name
     * @param InNPCType          npcType string from FNPCStruct, e.g. "merchant"
     * @param InLevel            NPC level (0 = hide level text)
     * @param InteractionState   Drives which indicator icon is shown
     */
    UFUNCTION(BlueprintCallable, Category = "NPC Nameplate")
    void SetNPCInfo(const FString&        InName,
                    const FString&        InNPCType,
                    int32                 InLevel,
                    ENPCInteractionState  InteractionState);

    /**
     * Show or hide the proximity interact hint ("Press F to talk").
     * Call every tick based on player-to-NPC distance vs. NPCData.radius.
     */
    UFUNCTION(BlueprintCallable, Category = "NPC Nameplate")
    void SetPlayerInRange(bool bInRange);

    /** Uniform scale applied via RenderTransform (called by UNPCNameplateComponent). */
    UFUNCTION(BlueprintCallable, Category = "NPC Nameplate")
    void SetWidgetScale(float Scale);

    /**
     * Fade opacity [0..1] driven by distance.
     * Applied directly to the widget render opacity so the widget stays
     * logically visible while smoothly fading - avoiding SetVisibility spam.
     */
    UFUNCTION(BlueprintCallable, Category = "NPC Nameplate")
    void SetNameplateOpacity(float Opacity);

    // ------------------------------------------------------------------ //
    //  Read-only state (useful for Blueprint logic)                        //
    // ------------------------------------------------------------------ //

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Nameplate")
    bool GetIsInteractable() const { return bCachedInteractable; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Nameplate")
    ENPCInteractionState GetInteractionState() const { return CachedInteractionState; }

    /** Start bobbing animation on the currently visible quest icon. No-op if no animation is bound. */
    UFUNCTION(BlueprintCallable, Category = "NPC Nameplate")
    void PlayQuestIconAnimation();

    /** Stop the bobbing animation. */
    UFUNCTION(BlueprintCallable, Category = "NPC Nameplate")
    void StopQuestIconAnimation();

protected:
    virtual void NativeConstruct() override;
    // ------------------------------------------------------------------ //
    //  Bound widgets - must match names in the Blueprint layout            //
    // ------------------------------------------------------------------ //

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UScaleBox* RootScaleBox;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* NPCNameText;

    /** NPC type label, e.g. "[Merchant]". Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* NPCTypeText;

    /** Level label, e.g. "Lv. 12". Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* NPCLevelText;

    /** Yellow "!" shown when a new quest is available. Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* QuestIndicatorImage;

    /** Gold "!" shown when a quest is ready to turn in (QuestComplete). Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* QuestDoneImage;

    /** Orange "!" shown while a quest is in progress. Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* QuestInProgressImage;

    /** Blue "?" shown for dialogue-only NPCs (no quest). Optional. Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* DialogueIndicatorImage;

    /** "Press F to interact" hint. Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* InteractHintText;

    /**
     * Bobbing animation created in WBP_NPCNameplate.
     * Name the animation "QuestIconBob" in the UMG animator.
     * Drives Translation Y of the quest icon images.
     */
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    TObjectPtr<UWidgetAnimation> QuestIconBob;

    // ------------------------------------------------------------------ //
    //  Configurable colours (set defaults in BP CDO)                       //
    // ------------------------------------------------------------------ //

    /** Name colour when NPC is interactable. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC Nameplate|Style")
    FLinearColor InteractableNameColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    /** Name colour when NPC is NOT interactable. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC Nameplate|Style")
    FLinearColor NonInteractableNameColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

    /** Text inserted before npcType, e.g. "[". */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC Nameplate|Style")
    FString TypePrefix = TEXT("[");

    /** Text inserted after npcType, e.g. "]". */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC Nameplate|Style")
    FString TypeSuffix = TEXT("]");

    /** Format for the level label, e.g. "Lv. {0}". Use {0} as the level placeholder. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC Nameplate|Style")
    FString LevelFormat = TEXT("Lv. {0}");

    /** Interact hint string shown when player is in range. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC Nameplate|Style")
    FString InteractHintString = TEXT("Press F to interact");

private:
    bool                 bCachedInteractable    = true;
    ENPCInteractionState CachedInteractionState = ENPCInteractionState::None;

    /** Returns the currently visible quest icon image, or nullptr if none active. */
    UImage* GetActiveQuestIcon() const;
};
