#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "BestiaryTierRowWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * UBestiaryTierRowWidget
 *
 * Base C++ class for one tier row inside BestiaryEntryWidget.
 * Create a Blueprint subclass (e.g. WBP_BestiaryTierRow), design the layout,
 * then assign it as TierRowClass on WBP_BestiaryEntryWidget.
 *
 * Blueprint subclass must provide:
 *   Tier_Category_Text  UTextBlock   (BindWidget)           — localized category name
 *   Tier_Content_Box    UVerticalBox (BindWidget)           — filled with data lines when unlocked
 *   Tier_Locked_Text    UTextBlock   (BindWidgetOptional)   — "???" hint when locked
 *   Tier_Status_Icon    UWidget      (BindWidgetOptional)   — checkmark / lock icon
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UBestiaryTierRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate this row with tier data.
     * Called by BestiaryEntryWidget::BuildTierRow after creating each row.
     */
    UFUNCTION(BlueprintCallable, Category = "Bestiary|Tier Row")
    void Setup(const FBestiaryTierStruct& Tier);

protected:
    /**
     * Called after all bound widgets are filled in by Setup().
     * Override in Blueprint to apply custom styling per categorySlug or locked state.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Bestiary|Tier Row")
    void OnSetupComplete(const FBestiaryTierStruct& Tier);

    // ------------------------------------------------------------------
    // Bound widgets — names must match the UMG widget names exactly
    // ------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Bestiary|Tier Row")
    UTextBlock* Tier_Category_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Bestiary|Tier Row")
    UVerticalBox* Tier_Content_Box = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary|Tier Row")
    UTextBlock* Tier_Locked_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary|Tier Row")
    UWidget* Tier_Status_Icon = nullptr;

private:
    /** Append text lines for unlocked tier data into Tier_Content_Box. */
    void PopulateTierContent(const FBestiaryTierStruct& Tier);

    /** Add a single text line to Tier_Content_Box. */
    void AddLine(const FString& Line);

    // Cached localization reference (valid for the widget's lifetime)
    class ULocalizationSubsystem* Loc = nullptr;
};
