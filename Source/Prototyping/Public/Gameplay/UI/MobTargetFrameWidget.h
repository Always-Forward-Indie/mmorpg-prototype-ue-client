#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Engine/Texture2D.h"
#include "Data/DataStructs.h"
#include "MobTargetFrameWidget.generated.h"

class UEffectSlotWidget;

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

    /**
     * Target castbar: server-driven progress, never local DeltaTime.
     * Hidden (Collapsed) for instant casts and the basic attack (see
     * FMobTargetCastRule). The interrupt icon stays Collapsed: casts are
     * non-interruptible server-side by design (tracker mirenhold #134 tracks
     * the per-skill flag; styling branches here once the server ships it).
     */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void ShowCast(const FString& SkillName, const FString& SkillSlug, float CastTimeSec, int64 CastStartedAtMs, bool bInterruptible = false);

    /** Single server-time progress step (called every NativeTick while casting). */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void TickCast();

    /** Hide the castbar immediately (result, interrupt, death, unlock). */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void HideCast();

    /**
     * Target status-effects row: one icon + remaining seconds per effect on
     * the target itself (source is not distinguished). Sorted by importance
     * (shield / invulnerability / enrage first, then by time), capped at 6
     * visible slots + "+N" overflow; the row collapses when empty.
     */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void UpdateStatusEffects(const TArray<FAppliedEffectData>& Effects);

    /** Update only the HP bar and text (called every Tick while target is locked). */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void UpdateHP(int32 CurrentHP, int32 MaxHP);

    /** Hide the widget and reset all fields. */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void ClearTarget();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // --- Bound widgets (must exist in the Blueprint with matching names) ---

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UImage* PortraitImage;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* MobNameText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* MobLevelText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    UProgressBar* MobHealthBar;

    // Optional HP value labels (split layout: "75%" large + "156 / 300" small).
    // When absent, UpdateHP falls back to the combined string in MobHealthText.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* MobHealthText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* MobHealthPctText = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* MobHealthValueText = nullptr;

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

    // Target castbar (slice 2): bar + skill name + remaining seconds + interrupt
    // icon. All optional — the frame works when the BP predates them.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UProgressBar* CastBar = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* CastNameText = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UTextBlock* CastTimeText = nullptr;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* InterruptIcon = nullptr;

    // Cast skill icon (same DT icon as the player skill list, via the skill repository).
    // Optional: hidden gracefully when the BP predates it.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UImage* CastSkillIcon = nullptr;

    // Target status-effects row (slice 3): icon + remaining seconds per effect.
    // Optional: hidden gracefully when the BP predates it.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UHorizontalBox* StatusEffectsBox = nullptr;

    /** Slot class for one effect icon (same WBP_EffectSlot as the player bar). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Frame|Effects")
    TSubclassOf<UEffectSlotWidget> EffectSlotClass;

    /** Effect definition table (icons / tints / tooltips), same asset as the player bar. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Frame|Effects")
    TObjectPtr<UDataTable> EffectDefinitionTable = nullptr;

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

    bool    bIsCasting = false;
    FString CachedCastSkillName;
    FString CachedCastSkillSlug;
    float   CachedCastTotalSec = 0.0f;
    int64   CachedCastStartedAtMs = 0;
    // Game-thread fallback anchor when the time-sync service is unavailable.
    double  FallbackCastStartWorldSec = 0.0;

    /** Server clock (chunk) in ms, or -1 when the time-sync service is missing. */
    int64 ResolveServerNowMs() const;

    /** Show/hide the castbar row widgets (never the frame itself). */
    void SetCastRowVisibility(ESlateVisibility Vis);

    /** Resolve the cast skill icon from the shared skill repository (async-safe). */
    void LoadCastIconFromRepo(const FString& SkillSlug);

    /** One cached target effect (grouped by slug, longest duration wins). */
    struct FCachedTargetEffect
    {
        FString Slug;
        FString EffectType;
        int32 Value = 0;
        double DurationSec = 0.0;
        // Server-clock expiry; <= 0 duration = permanent (never expires).
        int64 ExpiresAtServerMs = 0;
    };

    TArray<FCachedTargetEffect> CachedTargetEffects;
    double EffectsTickAccum = 0.0;
    // World-time arrival anchor for the no-timesync fallback path.
    double EffectsArrivedWorldSec = 0.0;

    /** Rebuild slot widgets from CachedTargetEffects (sorted, capped, +N). */
    void RebuildEffectSlots();

    /** Drop expired entries and refresh countdowns (once per second, frozen when dead). */
    void RefreshEffectTimers();

    /** Clear the row (new lock / unlock). */
    void ClearTargetEffects();

    /** Remaining seconds for a cached effect (server clock, world-time fallback). */
    double RemainingSecFor(const FCachedTargetEffect& E) const;
};
