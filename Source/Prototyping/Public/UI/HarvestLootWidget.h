#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Data/DataStructs.h"
#include "HarvestLootWidget.generated.h"

// Forward declarations
class UHarvestLootItemWidget;
class UItemTooltipWidget;
class UHarvestManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHarvestLootVisibilityChanged, bool, bIsVisible);

/**
 * Widget to display harvestable loot items with drag support and tooltips
 */
UCLASS()
class PROTOTYPING_API UHarvestLootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Initialize the widget
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void InitializeWidget();

	// Set the loot items to display
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void SetLootItems(const TArray<FHarvestItemStruct>& LootItems);

	// Clear all loot items
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void ClearLootItems();

	// Show/Hide the widget
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void ShowWidget();

	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void HideWidget();

	// Update remaining loot after pickup
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void UpdateRemainingLoot(const TArray<FHarvestItemStruct>& RemainingLoot);

	// Set harvest manager (called by UIManager)
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void SetHarvestManager(UHarvestManager* InHarvestManager);

	// Show/hide tooltip
	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void ShowTooltip(const FHarvestItemStruct& Item, FVector2D Position);

	UFUNCTION(BlueprintCallable, Category = "Harvest Loot")
	void HideTooltip();

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Harvest Loot Events")
	FOnHarvestLootVisibilityChanged OnHarvestLootVisibilityChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Drag functionality
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Handle window dragging
	void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

	// Button click handlers
	UFUNCTION()
	void OnPickupAllClicked();

	UFUNCTION()
	void OnCloseClicked();

	// Handle individual item pickup
	UFUNCTION()
	void OnItemPickupRequested(int32 ItemId, int32 Quantity);

	// Handle item hover events
	UFUNCTION()
	void OnItemHovered(int32 ItemId, bool bIsHovered);

protected:
	// UI Components (bind these in Blueprint)
	UPROPERTY(meta = (BindWidget))
	UScrollBox* ScrollBox_LootItems;

	// Alternative: WrapBox for grid layout (use instead of ScrollBox if you want grid layout)
	UPROPERTY(meta = (BindWidgetOptional))
	UWrapBox* WrapBox_LootItems;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_Title;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextBlock_ItemCount;

	UPROPERTY(meta = (BindWidget))
	UButton* Button_PickupAll;

	UPROPERTY(meta = (BindWidget))
	UButton* Button_Close;

	// Drag handle for moving the window
	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* DragHandle;

	// Widget class for individual loot items
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TSubclassOf<UHarvestLootItemWidget> LootItemWidgetClass;

	// Tooltip widget class
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Loot")
	TSubclassOf<UItemTooltipWidget> ItemTooltipWidgetClass;

	// Current tooltip widget
	UPROPERTY()
	UItemTooltipWidget* ItemTooltipWidget;

	// Layout settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Loot Settings")
	bool bUseGridLayout = true; // If true, uses WrapBox for grid layout; if false, uses ScrollBox for list layout

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Loot Settings")
	float SlotGap = 5.0f; // Gap between item slots

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Loot Settings")
	FVector2D SlotSize = FVector2D(80.0f, 80.0f); // Size of each item slot

private:
	// Current loot items
	TArray<FHarvestItemStruct> CurrentLootItems;

	// Individual loot item widgets
	TArray<UHarvestLootItemWidget*> LootItemWidgets;

	// Is widget currently visible
	bool bIsVisible;

	// Reference to HarvestManager
	UHarvestManager* HarvestManager;

	// Dragging state
	bool bDragging = false;
	FVector2D DragOffset = FVector2D::ZeroVector;
	FMargin DragPadding = FMargin(8.f);
	FVector2D CurrentViewportPosition = FVector2D::ZeroVector;

	// Tooltip state
	int32 HoveredItemId = -1;

	// Bind/unbind to HarvestManager events
	void BindToHarvestManager();
	void UnbindFromHarvestManager();

	// Event handlers
	UFUNCTION()
	void HandleHarvestCompleted(const FHarvestCompleteStruct& HarvestData);

	UFUNCTION()
	void HandleLootPickupSuccess(const FCorpseLootPickupResponseStruct& PickupData);

	UFUNCTION()
	void HandleLootPickupError(const FCorpseLootPickupErrorStruct& ErrorData);

	// Create individual loot item widgets
	void CreateLootItemWidgets();

	// Update item count display
	void UpdateItemCountDisplay();

	// Convert harvest item to inventory item for tooltip
	FInventoryItemStruct ConvertHarvestItemToInventoryItem(const FHarvestItemStruct& HarvestItem) const;

	// Get the appropriate container widget (WrapBox or ScrollBox)
	UPanelWidget* GetLootContainer() const;
};