// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "W_PlayerNameplateWidget.generated.h"

/**
 * World-space nameplate widget for other players (isOtherClient = true).
 *
 * Layout expected in the Blueprint (WBP_PlayerNameplate):
 *
 *   RootScaleBox  (ScaleBox)
 *     PlayerNameText    (TextBlock)   � character name
 *     PlayerClassText   (TextBlock)   � character class, e.g. "[Warrior]"  (BindWidgetOptional)
 *     PlayerLevelText   (TextBlock)   � level, e.g. "Lv. 42"               (BindWidgetOptional)
 *     DeadIcon          (Image)       � skull icon visible only when dead   (BindWidgetOptional)
 *     HPBar             (ProgressBar) � shown only during combat proximity  (BindWidgetOptional)
 *     HPText            (TextBlock)   � "1024 / 2000"                       (BindWidgetOptional)
 *
 * Call SetPlayerInfo()     once when the remote player spawns.
 * Call UpdateHealthBar()   when health data is received (on damage event).
 * Call SetDeadState()      when bIsDead changes.
 * Call SetWidgetScale()    from UPlayerNameplateComponent every tick.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UW_PlayerNameplateWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ------------------------------------------------------------------ //
    //  Primary update � call once after the remote player's data arrives  //
    // ------------------------------------------------------------------ //

    /**
     * Populate all nameplate fields from FCharacterDataStruct.
     *
     * @param InName      characterName
     * @param InClass     characterClass  (empty string = hide the class label)
     * @param InLevel     characterLevel
     * @param bIsDead     bIsDead flag from character data
     */
    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void SetPlayerInfo(const FString& InName,
                       const FString& InClass,
                       int32          InLevel,
                       bool           bIsDead);

    /**
     * Update the HP bar and text from a damage event or stats packet.
     * Automatically shows the bar for HpVisibleDuration seconds then hides it.
     *
     * @param CurrentHP   characterCurrentHealth
     * @param MaxHP       max health (from FPlayerStatsUpdateStruct or cached value)
     */
    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void UpdateHealthBar(int32 CurrentHP, int32 MaxHP);

    /**
     * Reflect a bIsDead state change without refreshing the whole nameplate.
     * Greys the name and shows the dead icon when bNewDead = true.
     */
    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void SetDeadState(bool bNewDead);

    /**
     * Update the title line under the player name.
     * Pass an empty string to hide the title.
     *
     * @param InTitle   Localised display name from FTitleEntry::displayName,
     *                  or empty string when no title is equipped.
     */
    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void SetTitle(const FString& InTitle);

    /** Uniform scale applied via RenderTransform (called by UPlayerNameplateComponent). */
    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void SetWidgetScale(float Scale);

    // ------------------------------------------------------------------ //
    //  Tick � must be called every frame to handle the HP bar auto-hide   //
    // ------------------------------------------------------------------ //
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ------------------------------------------------------------------ //
    //  Read-only state                                                     //
    // ------------------------------------------------------------------ //

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Nameplate")
    bool GetIsDead() const { return bCachedDead; }

protected:
    // ------------------------------------------------------------------ //
    //  Bound widgets                                                       //
    // ------------------------------------------------------------------ //

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UScaleBox* RootScaleBox;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* PlayerNameText;

    /** Class label, e.g. "[Warrior]". Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* PlayerClassText;

    /** Level label, e.g. "Lv. 42". Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* PlayerLevelText;

    /** Skull / dead icon. Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* DeadIcon;

    /**
     * HP bar � shown for HpVisibleDuration seconds after UpdateHealthBar() is called,
     * then auto-hides. Optional.
     */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UProgressBar* HPBar;

    /** HP value label "1024 / 2000". Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* HPText;

    /** Title label shown below the name, e.g. "Wolf Slayer". Hidden when empty. Optional. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* TitleText;

    // ------------------------------------------------------------------ //
    //  Configurable style (set defaults in BP CDO)                         //
    // ------------------------------------------------------------------ //

    /** Normal name colour. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Nameplate|Style")
    FLinearColor AliveNameColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    /** Name colour when the player is dead. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Nameplate|Style")
    FLinearColor DeadNameColor = FLinearColor(0.45f, 0.45f, 0.45f, 1.0f);

    /** Prefix/suffix around the class string, e.g. "[" / "]". */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Nameplate|Style")
    FString ClassPrefix = TEXT("[");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Nameplate|Style")
    FString ClassSuffix = TEXT("]");

    /** Format for the level label. Use {0} as placeholder. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Nameplate|Style")
    FString LevelFormat = TEXT("Lv. {0}");

    /**
     * Seconds the HP bar stays visible after receiving a health update.
     * Set to 0 to keep the bar always hidden (pure name-only nameplate).
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Nameplate|HP Bar")
    float HpVisibleDuration = 4.0f;

    /** HP bar fill colour. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Nameplate|HP Bar")
    FLinearColor HPBarColor = FLinearColor(0.18f, 0.72f, 0.18f, 1.0f);

private:
    bool  bCachedDead   = false;
    float HpHideTimer   = 0.0f;
    bool  bHPBarVisible = false;

    void ShowHPBar(bool bShow);
};
