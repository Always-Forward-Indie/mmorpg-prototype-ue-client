#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Data/DataStructs.h"
#include "QuantityPopupWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuantityConfirmed, int32, SlotIndex, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnQuantityRemove,    int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE          (FOnQuantityCancelled);

/**
 * UQuantityPopupWidget
 *
 * Small popup that lets the player choose a quantity before adding to cart
 * or updating an existing cart entry.
 *
 * Blueprint must bind:
 *   ItemNameText      UTextBlock
 *   PricePerUnitText  UTextBlock
 *   TotalPriceText    UTextBlock
 *   QuantityInput     UEditableTextBox
 *   DecreaseButton    UButton
 *   IncreaseButton    UButton
 *   MaxButton         UButton         (BindWidgetOptional)
 *   ConfirmButton     UButton
 *   RemoveButton      UButton         (BindWidgetOptional) — shown only in update mode
 *   CancelButton      UButton
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UQuantityPopupWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Open in "add to cart" mode
    UFUNCTION(BlueprintCallable, Category = "Quantity Popup")
    void OpenForAdd(int32 InSlotIndex, const FString& InName, int32 InPricePerUnit,
                    int32 InMaxQuantity, int32 InInitialQuantity = 1);

    // Open in "update cart entry" mode — shows Remove button
    UFUNCTION(BlueprintCallable, Category = "Quantity Popup")
    void OpenForUpdate(int32 InSlotIndex, const FString& InName, int32 InPricePerUnit,
                       int32 InMaxQuantity, int32 InCurrentQuantity);

    UFUNCTION(BlueprintCallable, Category = "Quantity Popup")
    void ClosePopup();

    // -----------------------------------------------------------------------
    // Events
    // -----------------------------------------------------------------------

    // Fired when Confirm is clicked — carries SlotIndex + chosen quantity
    UPROPERTY(BlueprintAssignable, Category = "Quantity Popup Events")
    FOnQuantityConfirmed OnQuantityConfirmed;

    // Fired when Remove is clicked (update mode only)
    UPROPERTY(BlueprintAssignable, Category = "Quantity Popup Events")
    FOnQuantityRemove OnQuantityRemove;

    // Fired when Cancel is clicked
    UPROPERTY(BlueprintAssignable, Category = "Quantity Popup Events")
    FOnQuantityCancelled OnQuantityCancelled;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* ItemNameText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* PricePerUnitText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* TotalPriceText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UEditableTextBox* QuantityInput = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* DecreaseButton = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* IncreaseButton = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* MaxButton = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* ConfirmButton = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* RemoveButton = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* CancelButton = nullptr;

private:
    int32 SlotIndex     = -1;
    int32 PricePerUnit  = 0;
    int32 MaxQuantity   = 1;
    int32 CurrentQty    = 1;
    bool  bUpdateMode   = false;

    void SetQuantity(int32 NewQty);
    void UpdateTotalPrice();

    UFUNCTION() void HandleDecrease();
    UFUNCTION() void HandleIncrease();
    UFUNCTION() void HandleMax();
    UFUNCTION() void HandleConfirm();
    UFUNCTION() void HandleRemove();
    UFUNCTION() void HandleCancel();
    UFUNCTION() void HandleQuantityTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
