#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "BestiaryMobRowWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBestiaryMobRowSelected, const FString&, MobSlug);

/**
 * UBestiaryMobRowWidget
 *
 * Base C++ class for one row in the bestiary mob list.
 * Create a Blueprint subclass (e.g. WBP_BestiaryMobRow), design the layout,
 * then assign it as MobRowClass on WBP_BestiaryWidget.
 *
 * Blueprint subclass must provide:
 *   Row_Mob_Name_Text   UTextBlock  (BindWidget)           — localized mob name
 *   Row_Kill_Text       UTextBlock  (BindWidgetOptional)   — kill count badge
 *   Row_Mob_Icon        UImage      (BindWidgetOptional)   — mob thumbnail
 *   Row_Select_Btn      UButton     (BindWidgetOptional)   — clicking fires OnMobRowSelected
 *
 * If Row_Select_Btn is absent the whole widget can be made clickable via
 * NativeOnMouseButtonDown in the Blueprint subclass.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UBestiaryMobRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate this row with overview data.
     * Called by BestiaryWidget::RebuildMobList after creating each row.
     */
    UFUNCTION(BlueprintCallable, Category = "Bestiary|Mob Row")
    void Setup(const FString& InMobSlug, int32 InKillCount);

    /** The mob slug this row represents (set by Setup). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bestiary|Mob Row")
    const FString& GetMobSlug() const { return MobSlug; }

    /** Fired when the player clicks this row (or Row_Select_Btn). */
    UPROPERTY(BlueprintAssignable, Category = "Bestiary|Mob Row|Events")
    FOnBestiaryMobRowSelected OnMobRowSelected;

protected:
    virtual void NativeConstruct() override;

    /**
     * Called after bound widgets are filled in by Setup().
     * Override in Blueprint to apply custom styling (e.g. highlight selected row).
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Bestiary|Mob Row")
    void OnSetupComplete(const FString& InMobSlug, int32 InKillCount);

    // ------------------------------------------------------------------
    // Bound widgets — names must match the UMG widget names exactly
    // ------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Bestiary|Mob Row")
    UTextBlock* Row_Mob_Name_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary|Mob Row")
    UTextBlock* Row_Kill_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary|Mob Row")
    UImage* Row_Mob_Icon = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary|Mob Row")
    UButton* Row_Select_Btn = nullptr;

private:
    UFUNCTION()
    void HandleSelectClicked();

    FString MobSlug;
};
