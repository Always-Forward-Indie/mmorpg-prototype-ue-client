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

	// Bind these in your Blueprint widget
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* ItemNameText;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* QuantityInput;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

private:
	UPROPERTY()
	FInventoryItemStruct PendingItem;

	int32 MaxQuantity = 1;

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnCancelClicked();
};
