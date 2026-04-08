#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "TitleRowWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * UTitleRowWidget
 *
 * One row in the Titles list window.
 * Create a Blueprint subclass, lay out the widgets below, and set this class
 * as the TitleRowClass on your TitlesWidget Blueprint.
 *
 * Required (BindWidget):
 *   Row_Name_Text    UTextBlock  — title display name (prefixed with "[E] " when equipped)
 *   Row_Equip_Button UButton     — equip button; disabled when title is already equipped
 *
 * Optional (BindWidgetOptional):
 *   Row_Bonus_Text   UTextBlock  — bonus summary e.g. "+2 Physical Attack, +1 Move Speed"
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UTitleRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate and wire the row.
     * Called by UTitlesWidget::AddTitleRow().
     */
    void Populate(const FTitleEntry& Entry, bool bIsEquipped);

    // --- Required bound widgets ---
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Title Row")
    UTextBlock* Row_Name_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Title Row")
    UButton* Row_Equip_Button = nullptr;

    // --- Optional bound widgets ---
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Title Row")
    UTextBlock* Row_Bonus_Text = nullptr;

protected:
    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void HandleEquipClicked();

    static FString BuildBonusSummary(const TArray<FTitleBonusEntry>& Bonuses);

    FString RowSlug;
    bool    bRowIsEquipped = false;

public:
    /** Fired when the player clicks Equip; payload is the slug to equip (empty = remove). */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTitleRowEquipRequested, const FString&, TitleSlug);
    UPROPERTY(BlueprintAssignable, Category = "Title Row|Events")
    FOnTitleRowEquipRequested OnEquipRequested;
};
