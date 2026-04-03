#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "ItemTooltipWidget.generated.h"

/**
 * Tooltip widget that displays detailed item information when hovering over inventory slots
 */
UCLASS()
class PROTOTYPING_API UItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UItemTooltipWidget(const FObjectInitializer& ObjectInitializer);

	// Set item data to display in tooltip
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void SetItemData(const FInventoryItemStruct& Item);

	// Update tooltip position
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateTooltipPosition(FVector2D ScreenPosition);

	// Show tooltip with animation
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void ShowTooltip();

	// Hide tooltip with animation
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void HideTooltip();

protected:
	virtual void NativeConstruct() override;

	// Update all tooltip content
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateTooltipContent();

	// Update item name and rarity
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateItemHeader();

	// Update item description
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateItemDescription();

	// Update item attributes/stats
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateItemAttributes();

	// Update item durability
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateItemDurability();

	// Update equip slot info
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateEquipInfo();

	// Update vendor prices
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateVendorPrices();

	// Update use effects (consumables)
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateUseEffects();

	// Update item type and level
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateItemTypeAndLevel();

	// Update item weight
	UFUNCTION(BlueprintCallable, Category = "Item Tooltip")
	void UpdateItemWeight();

	// Get rarity color
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Tooltip")
	FLinearColor GetRarityColor(const FString& Rarity) const;

	// Get type color
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Tooltip")
	FLinearColor GetTypeColor(const FString& Type) const;

	// Get weight color based on weight value
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Tooltip")
	FLinearColor GetWeightColor(float Weight) const;

	// Format weight text with units
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Tooltip")
	FText FormatWeightText(float Weight) const;

	// Format attribute text
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Tooltip")
	FText FormatAttributeText(const FString& AttributeName, const FString& AttributeValue) const;

protected:
	// UI Components (bind these in Blueprint)
	UPROPERTY(meta = (BindWidget))
	UBorder* TooltipBorder;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* MainContent;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HeaderBox;

	UPROPERTY(meta = (BindWidget))
	UImage* ItemIcon;

	UPROPERTY(meta = (BindWidget))
	UImage* Separator4;

	UPROPERTY(meta = (BindWidget))
	UImage* Separator3;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ItemNameText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ItemRarityText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ItemTypeText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ItemLevelText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ItemDescriptionText;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* AttributesBox;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* QuantityText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* WeightText;

	// Optional bindings — add these widgets in Blueprint to enable the section
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* DurabilityText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* EquipSlotText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* VendorPriceText;

	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* UseEffectsBox;

	// Current item data
	UPROPERTY(BlueprintReadOnly, Category = "Item Tooltip")
	FInventoryItemStruct CurrentItem;

	// Visual settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	FVector2D TooltipOffset = FVector2D(10.0f, -10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	float FadeInDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	float FadeOutDuration = 0.1f;

	// Rarity colors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	TMap<FString, FLinearColor> RarityColors;

	// Type colors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	TMap<FString, FLinearColor> TypeColors;

	// Weight display settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	FString WeightUnit = TEXT("kg");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	bool bShowWeightColorCoding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	bool bShowWeightUnit = true;

	// Weight color coding thresholds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip", meta = (EditCondition = "bShowWeightColorCoding"))
	float LightWeightThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip", meta = (EditCondition = "bShowWeightColorCoding"))
	float MediumWeightThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip", meta = (EditCondition = "bShowWeightColorCoding"))
	float HeavyWeightThreshold = 100.0f;

	// Attribute text widget class (for dynamic attributes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Tooltip")
	TSubclassOf<UTextBlock> AttributeTextWidgetClass;

private:
	// Initialize colors
	void InitializeColors();

	// Clear dynamic attributes
	void ClearDynamicAttributes();

	// Add attribute text widget
	UTextBlock* AddAttributeTextWidget(const FString& AttributeName, const FString& AttributeValue);

	// Format attribute name for display
	FString FormatAttributeName(const FString& AttributeName) const;

	// Check if tooltip is currently visible
	bool bIsVisible = false;

	/** Handle for async texture loading */
	TSharedPtr<FStreamableHandle> StreamableHandle;

	/** Sets the icon texture */
	void SetIconTexture(UTexture2D* Texture);

	/** Sets default/placeholder icon */
	void SetDefaultIcon();

	/** Helper function for async asset loading */
	void AsyncLoad(const FSoftObjectPath& Path, FSimpleDelegate Callback);

	//load item icon
	void LoadItemIcon();
};