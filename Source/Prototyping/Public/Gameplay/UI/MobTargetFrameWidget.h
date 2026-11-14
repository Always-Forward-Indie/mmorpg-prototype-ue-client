#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "MobTargetFrameWidget.generated.h"

/**
 * HUD widget that shows the hard-locked combat target's info:
 * icon (from FMobVisualData::Icon), localized name, rank, level and HP bar,
 * plus the target's own castbar and status-effects row.
 *
 * One class for everything (normal / elite / champion / boss): the boss frame
 * is just a second instance of this class with a fixed-size layout.
 *
 * Usage:
 *   - Add as BindWidgetOptional to W_PlayerInterface, Visibility=Collapsed by default
 *   - Call SetMobInfo() when a target is locked (resets rank/dead/cast/effects state)
 *   - Call SetRank() / SetDead() right after SetMobInfo for mob targets
 *   - Call UpdateHP() every frame while target is alive
 *   - Call ClearTarget() when the lock is released
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UMobTargetFrameWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate all fields and make the widget visible.
     * @param MobSlug     Server slug used for localization lookup (e.g. "forest_wolf")
     * @param MobName     Raw fallback name shown when localization key is missing
     * @param MobLevel    Level shown with the viewer-diff colour
     * @param CurrentHP   Current HP value
     * @param MaxHP       Max HP value
     * @param bIsAggro    True = mob is aggressive (red name), false = passive (yellow)
     * @param Icon        Icon texture from FMobVisualData::Icon; nullptr shows default
     */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void SetMobInfo(const FString& MobSlug,
                    const FString& MobName,
                    int32 MobLevel,
                    int32 CurrentHP, int32 MaxHP,
                    bool bIsAggro,
                    UTexture2D* Icon = nullptr);

    /**
     * Rank chrome: label + frame border + level colour for the viewer diff.
     * Call after SetMobInfo for mob targets. Empty RankCode hides the rank row
     * (player/self targets carry no rank — never show a fake "normal").
     * @param RankCode      Server mobRankCode ("normal"/"elite"/"boss"/"champion")
     * @param bIsChampion  Server isChampion flag (wins over a stale code)
     * @param ChampionKind  Server championKind, appended raw ("Champion • <kind>")
     * @param ViewerLevel   Local player level for the level-diff colour (<=0 = neutral)
     */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void SetRank(const FString& RankCode, bool bIsChampion, const FString& ChampionKind, int32 ViewerLevel);

    /** Dead state: grey HP, portrait-only overlay + dead label, castbar hidden. */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void SetDead(bool bIsDead);

    /** Update only the HP bar and text (called every Tick while target is locked). */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void UpdateHP(int32 CurrentHP, int32 MaxHP);

    /** Hide the widget and reset all fields. */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void ClearTarget();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- Bound widgets (must exist in the Blueprint with matching names) ---

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UImage* PortraitImage;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* MobNameText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* MobLevelText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UProgressBar* MobHealthBar;

    // Optional HP value label "156 / 300"
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* MobHealthText;

    // Optional aggro indicator icon (e.g. a red skull)
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* AggroIcon;

    // Rank chrome: text label (localized) + frame border (fixed colour).
    // Optional: hidden gracefully when the BP predates them.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* RankLabelText = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UBorder* RankBorder = nullptr;

    // Death chrome: 50% overlay over the portrait only + label on top of it.
    // Optional: HP bar still goes grey when the BP predates them.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* DeadOverlay = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* DeadText = nullptr;

    // Default icon shown when no texture is assigned in the data table
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Target Frame")
    UTexture2D* DefaultIcon = nullptr;

private:
    /** Apply a resolved texture to PortraitImage. */
    void ApplyPortraitTexture(UTexture2D* Texture);

    /**
     * Look up the mob's icon in MobDefinitionTable by slug and apply it.
     * Uses the synchronous hot-path if the texture is already resident;
     * falls back to async streaming otherwise (non-blocking).
     */
    void LoadPortraitFromTable(const FString& MobSlug);

    /** Refresh localized texts (name + rank + dead) from the cached state. */
    void RefreshLocalizedTexts();

    /** (Re)apply rank chrome from the cached rank state. */
    void ApplyRankChrome();

    /** (Re)apply dead chrome from the cached dead flag. */
    void ApplyDeadChrome();

    UFUNCTION()
    void HandleLocaleChanged(const FString& NewLocale);

    FString CachedMobSlug;
    FString CachedMobName;
    int32   CachedMobLevel = 0;
    int32   CachedCurrentHP = 0;
    int32   CachedMaxHP = 0;
    bool    bCachedAggro = false;
    bool    bHasTarget = false;

    // Last resolved portrait (caller-supplied or table-loaded): re-applied on
    // locale change instead of wiping the brush and re-streaming (no flash).
    UPROPERTY()
    UTexture2D* CachedPortrait = nullptr;

    FString CachedRankCode;
    bool    bCachedChampion = false;
    FString CachedChampionKind;
    int32   CachedViewerLevel = 0;
    bool    bCachedDead = false;
};
