#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Data/DataStructs.h"
#include "UI/BestiaryTierRowWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "BestiaryEntryWidget.generated.h"

class UTextBlock;
class UScrollBox;
class UVerticalBox;
class UWidgetSwitcher;
class UButton;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBestiaryEntryCloseRequested);

/**
 * BestiaryEntryWidget
 *
 * Detail panel shown when the player selects a mob in the bestiary list.
 * Displays mob name, kill count, and all tiers (unlocked data or "???" for locked).
 *
 * Blueprint subclass must bind:
 *   Mob_Name_Text       UTextBlock   � localized mob name
 *   Mob_Description_Text UTextBlock  � localized mob description (BindWidgetOptional)
 *   Kill_Count_Text     UTextBlock   � "Kills: N"
 *   Tiers_Box           UVerticalBox � populated dynamically per tier
 *   Close_Button        UButton      (BindWidgetOptional) � hides the panel
 *
 * Each tier row is created from TierRowClass (set in Blueprint).
 * The row widget needs:
 *   Tier_Category_Text  UTextBlock   � localized categorySlug
 *   Tier_Status_Icon    UWidget      � checkmark or lock icon (BindWidgetOptional)
 *   Tier_Content_Box    UVerticalBox � filled with unlocked data lines
 *   Tier_Locked_Text    UTextBlock   � "???" or "N more kills" hint (BindWidgetOptional)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UBestiaryEntryWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate the widget with the given bestiary entry.
     * Call after receiving OnBestiaryEntryReceived.
     */
    UFUNCTION(BlueprintCallable, Category = "Bestiary Entry")
    void DisplayEntry(const FBestiaryEntryStruct& Entry);

    /** Clear all content (called when closing or switching entries). */
    UFUNCTION(BlueprintCallable, Category = "Bestiary Entry")
    void ClearEntry();

    UPROPERTY(BlueprintAssignable, Category = "Bestiary Entry|Events")
    FOnBestiaryEntryCloseRequested OnCloseRequested;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Drag support
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    UFUNCTION()
    void HandleCloseClicked();

    // ------------------------------------------------------------------
    // Bound widgets (names must match UMG widget names in Blueprint)
    // ------------------------------------------------------------------
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Bestiary Entry")
    UTextBlock* Mob_Name_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary Entry")
    UTextBlock* Mob_Description_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary Entry")
    UImage* Mob_Icon = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Bestiary Entry")
    UTextBlock* Kill_Count_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Bestiary Entry")
    UVerticalBox* Tiers_Box = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary Entry")
    UButton* Close_Button = nullptr;

    /** Drag handle — if unbound, the entire widget is draggable. */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary Entry")
    UWidget* DragHandle = nullptr;

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    /** Row widget class used to display each tier. Must be a subclass of UBestiaryTierRowWidget. Set in Blueprint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Entry|Config")
    TSubclassOf<UBestiaryTierRowWidget> TierRowClass;

private:
/** Build one tier row widget and add it to Tiers_Box. */
void BuildTierRow(const FBestiaryTierStruct& Tier);

UFUNCTION()
void HandleLocaleChanged(const FString& NewLocale);

FBestiaryEntryStruct CurrentEntry;

// Drag support
bool      bDragging = false;
FVector2D DragOffset = FVector2D::ZeroVector;
FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
};
