#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Data/DataStructs.h"
#include "DropQuantityPopupWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDropQuantityConfirmed, const FInventoryItemStruct&, Item, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDropQuantityCancelled);

/**
 * Modal popup that asks the player how many items to drop.
 * Subclass in Blueprint, bind: ItemNameText, QuantityInput, ConfirmButton, CancelButton.
 */
UCLASS()
class PROTOTYPING_API UDropQuantityPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Show the popup for the given item (max = item.quantity)
	UFUNCTION(BlueprintCallable, Category = "Drop Quantity Popup")
	void ShowForItem(const FInventoryItemStruct& InItem);

	// Hide the popup
	UFUNCTION(BlueprintCallable, Category = "Drop Quantity Popup")
	void HidePopup();

	// Fired with the item and chosen quantity when the player confirms
	UPROPERTY(BlueprintAssignable, Category = "Drop Quantity Popup|Events")
	FOnDropQuantityConfirmed OnDropQuantityConfirmed;

	// Fired when the player cancels
	UPROPERTY(BlueprintAssignable, Category = "Drop Quantity Popup|Events")
	FOnDropQuantityCancelled OnDropQuantityCancelled;

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Bind these in your Blueprint widget
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* ItemNameText;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* QuantityInput;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* MaxButton;

	UPROPERTY(meta = (BindWidget))
	UButton* DecreaseButton;

	UPROPERTY(meta = (BindWidget))
	UButton* IncreaseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* PopupContent = nullptr;

private:
	UPROPERTY()
	FInventoryItemStruct PendingItem;

	int32 MaxQuantity = 1;
	int32 CurrentQty = 1;

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnCancelClicked();

	UFUNCTION()
	void HandleMax();

	UFUNCTION()
	void HandleDecrease();

	UFUNCTION()
	void HandleIncrease();

	UFUNCTION()
	void HandleQuantityTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleQuantityTextChanged(const FText& Text);

	void SetQuantity(int32 NewQty);
};
