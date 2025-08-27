#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Data/DataStructs.h"
#include <Components/SizeBox.h>
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "InventorySlotWidget.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotRightClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotHovered, int32, SlotIndex, bool, bIsHovered);

/**
 * Individual inventory slot widget that represents a single item slot
 */
UCLASS()
class PROTOTYPING_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventorySlotWidget(const FObjectInitializer& ObjectInitializer);

	// Initialize the slot with index
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void InitializeSlot(int32 InSlotIndex);

	// Set item data for this slot
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void SetItemData(const FInventoryItemStruct& Item);

	// Clear the slot
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void ClearSlot();

	// Get current item data
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	FInventoryItemStruct GetItemData() const { return CurrentItem; }

	// Check if slot has item
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	bool HasItem() const { return CurrentItem.itemId > 0; }

	// Get slot index
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	int32 GetSlotIndex() const { return SlotIndex; }

	// Set slot highlight
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void SetSlotHighlight(bool bHighlighted);

	// Set slot selection
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void SetSlotSelected(bool bSelected);

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Inventory Slot Events")
	FOnSlotClicked OnSlotClicked;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Slot Events")
	FOnSlotRightClicked OnSlotRightClicked;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Slot Events")
	FOnSlotHovered OnSlotHovered;

	UPROPERTY(meta = (BindWidget)) USizeBox* SlotSizeBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true", ClampMin = "8"))
	float SlotSide = 64.f;

	UFUNCTION(BlueprintCallable)
	void SetSlotSide(float InSide);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativePreConstruct() override;

	// Update visual appearance
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void UpdateSlotAppearance();

	// Load item icon
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void LoadItemIcon();

	// Update quantity text
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void UpdateQuantityText();

	// Get rarity color
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	FLinearColor GetRarityColor(const FString& Rarity) const;

protected:
	// UI Components (bind these in Blueprint)
	UPROPERTY(meta = (BindWidget))
	UButton* SlotButton;

	UPROPERTY(meta = (BindWidget))
	UImage* ItemIcon;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* QuantityText;

	UPROPERTY(meta = (BindWidget))
	UBorder* SlotBorder;

	UPROPERTY(meta = (BindWidget))
	UBorder* RarityBorder;

	UPROPERTY(meta = (BindWidget))
	UImage* ImgStroke;

	UPROPERTY(meta = (BindWidget))
	UImage* ImgSelected;

	UPROPERTY(meta = (BindWidget))
	UImage* ImgOccupied;

	UPROPERTY(meta = (BindWidget))
	UImage* ImgBase;

	// Slot data
	UPROPERTY(BlueprintReadOnly, Category = "Inventory Slot")
	FInventoryItemStruct CurrentItem;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory Slot")
	int32 SlotIndex = -1;

	// Visual states
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	bool bIsHighlighted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	bool bIsSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	bool bIsHovered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	bool bIsOccupied = false;

	// Visual settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	FLinearColor DefaultBorderColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	FLinearColor HighlightBorderColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	FLinearColor SelectedBorderColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	FLinearColor HoveredBorderColor = FLinearColor(0.5f, 0.5f, 1.0f, 1.0f);

	// Rarity colors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Slot")
	TMap<FString, FLinearColor> RarityColors;

private:
	// Initialize rarity colors
	void InitializeRarityColors();

	// Handle button events
	UFUNCTION()
	void OnSlotButtonClicked();

	UFUNCTION()
	void OnSlotButtonHovered();

	UFUNCTION()
	void OnSlotButtonUnhovered();


	/** Handle for async texture loading */
	TSharedPtr<FStreamableHandle> StreamableHandle;

	/** Sets the icon texture */
	void SetIconTexture(UTexture2D* Texture);

	/** Sets default/placeholder icon */
	void SetDefaultIcon();

	/** Helper function for async asset loading */
	void AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback);
};