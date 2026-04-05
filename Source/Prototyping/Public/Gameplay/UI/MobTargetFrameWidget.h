#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "MobTargetFrameWidget.generated.h"

/**
 * HUD widget that shows the hard-locked combat target's info:
 * icon (from FMobVisualData::Icon), localized name, level and HP bar.
 *
 * Usage:
 *   - Add as BindWidgetOptional to WBP_PlayerInterface, Visibility=Collapsed by default
 *   - Call SetMobInfo() when a target is locked
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
     * @param MobLevel    Level shown as "Lv. N"
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

    /** Update only the HP bar and text (called every Tick while target is locked). */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void UpdateHP(int32 CurrentHP, int32 MaxHP);

    /** Hide the widget and reset all fields. */
    UFUNCTION(BlueprintCallable, Category = "Target Frame")
    void ClearTarget();

protected:
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
};

