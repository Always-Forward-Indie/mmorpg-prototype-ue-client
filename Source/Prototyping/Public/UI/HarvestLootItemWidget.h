#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Data/DataStructs.h"
#include "Engine/StreamableManager.h"
#include "HarvestLootItemWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemPickupRequested, int32, ItemId, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemHovered, int32, ItemId, bool, bIsHovered);

/**
 * Widget to display individual harvest loot item with icon loading and hover support
 */
UCLASS()
class PROTOTYPING_API UHarvestLootItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Initialize the widget with item data
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot Item")
	void SetItemData(const FHarvestItemStruct& ItemData);

	// Get the current item data
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Harvest Loot Item")
	const FHarvestItemStruct& GetItemData() const { return CurrentItemData; }

	// Set widget size for grid layout
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot Item")
	void SetSlotSize(float InSize);

	// Event delegate for item pickup
	UPROPERTY(BlueprintAssignable, Category = "Harvest Loot Item")
	FOnItemPickupRequested OnItemPickupRequested;

	// Event delegate for item hover
	UPROPERTY(BlueprintAssignable, Category = "Harvest Loot Item")
	FOnItemHovered OnItemHovered;

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Button click handler
	UFUNCTION()
	void OnPickupButtonClicked();

	// Update item display
	void UpdateItemDisplay();

	// Load item icon from data table
	void LoadItemIcon();

	// Set icon texture
	void SetIconTexture(UTexture2D* Texture);

	// Set default icon
	void SetDefaultIcon();

	// Helper function for async loading
	void AsyncLoad(const FSoftObjectPath& Path, FStreamableDelegate Callback);

	// Get rarity color
	FLinearColor GetRarityColor(int32 RarityId) const;

protected:
	// UI Components (bind these in Blueprint)
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextBlock_ItemName;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextBlock_ItemType;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextBlock_Quantity;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextBlock_Description;

	UPROPERTY(meta = (BindWidget))
	class UImage* Image_ItemIcon;

	UPROPERTY(meta = (BindWidget))
	class UImage* Image_RarityBorder;

	UPROPERTY(meta = (BindWidget))
	class UButton* Button_Pickup;

	// Optional size box for controlling widget size
	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* SlotSizeBox;

	// Layout settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float SlotSize = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bUseCompactLayout = false; // If true, uses compact grid layout; if false, uses full item info layout

private:
	// Current item data
	FHarvestItemStruct CurrentItemData;

	// Handle for async texture loading
	TSharedPtr<FStreamableHandle> StreamableHandle;
};