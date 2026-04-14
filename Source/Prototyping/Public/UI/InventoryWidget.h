#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/GridPanel.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Data/DataStructs.h"
#include "UI/InventorySlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "UI/ItemActionMenuWidget.h"
#include "UI/DropQuantityPopupWidget.h"
#include <Components/WrapBox.h>
#include <Components/HorizontalBox.h>
#include "Math/Color.h"
#include "InventoryWidget.generated.h"

// Forward declarations
class UInventoryManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventorySlotClicked, int32, SlotIndex, const FInventoryItemStruct&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventorySlotRightClicked, int32, SlotIndex, const FInventoryItemStruct&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryVisibilityChanged, bool, bIsVisible);

/**
 * Main inventory UI widget that displays the player's inventory in a grid layout
 */
UCLASS()
class PROTOTYPING_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventoryWidget(const FObjectInitializer& ObjectInitializer);

	// Initialize the inventory widget with inventory manager reference
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void InitializeInventory(UInventoryManager* InInventoryManager);

	// Update the inventory display
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void UpdateInventoryDisplay(const FCharacterInventoryStruct& Inventory);

	// Set inventory grid size
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetInventorySize(int32 Rows, int32 Columns);

	// Show/hide inventory
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetInventoryVisible(bool bVisible);

	// Toggle inventory visibility
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ToggleInventory();

	// Get inventory visibility
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	bool IsInventoryVisible() const;

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Inventory UI Events")
	FOnInventorySlotClicked OnInventorySlotClicked;

	UPROPERTY(BlueprintAssignable, Category = "Inventory UI Events")
	FOnInventorySlotRightClicked OnInventorySlotRightClicked;

	UPROPERTY(BlueprintAssignable, Category = "Inventory UI Events")
	FOnInventoryVisibilityChanged OnInventoryVisibilityChanged;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ������ ��� ��������� �������
	void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

	// �������������� ������� ����
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;


	// Create inventory grid
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void CreateInventoryGrid();

	// Clear inventory grid
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ClearInventoryGrid();

	// Get slot widget at index
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	UInventorySlotWidget* GetSlotWidget(int32 SlotIndex) const;

	// Update slot display
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void UpdateSlot(int32 SlotIndex, const FInventoryItemStruct& Item);

	// Clear slot display
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ClearSlot(int32 SlotIndex);

	// Handle slot events
	UFUNCTION()
	void OnSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void OnSlotRightClicked(int32 SlotIndex);

	UFUNCTION()
	void OnSlotHovered(int32 SlotIndex, bool bIsHovered);

	// Inventory manager callbacks
	UFUNCTION()
	void OnInventoryUpdated(const FCharacterInventoryStruct& UpdatedInventory);

	UFUNCTION()
	void OnItemAdded(const FInventoryItemStruct& Item, int32 Quantity);

	UFUNCTION()
	void OnItemRemoved(const FInventoryItemStruct& Item, int32 Quantity);

	// Show/hide tooltip
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ShowTooltip(const FInventoryItemStruct& Item, FVector2D Position);

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void HideTooltip();

	// Open the right-click context menu for a slot
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void OpenContextMenu(const FInventoryItemStruct& Item, FVector2D ScreenPosition);

	// Close context menu if open
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void CloseContextMenu();

	// Unified context menu action handler (dispatches drop to quantity popup)
	UFUNCTION()
	void HandleContextAction(EItemContextAction Action, const FInventoryItemStruct& Item);

	// Drop quantity popup handlers
	UFUNCTION()
	void HandleDropConfirmed(const FInventoryItemStruct& Item, int32 Quantity);

	UFUNCTION()
	void HandleDropCancelled();

	// Calculate inventory statistics
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	float GetCurrentInventoryWeight() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	int32 GetUsedSlots() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	int32 GetFreeSlots() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void UpdateInventoryStats();

	// Set max weight from character attributes
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetMaxInventoryWeight(float NewMaxWeight);

protected:
	// UI Components (bind these in Blueprint)
	// ������ GridPanel
	UPROPERTY(meta = (BindWidget))
	UWrapBox* InventoryWrap;

	// ���������
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	float SlotGap = 5.f;           // ������ ����� ��������

	UPROPERTY(meta = (BindWidget))
	UTextBlock* InventoryTitle;

	UPROPERTY(meta = (BindWidget))
	UButton* CloseButton;

	// Weight and slots display
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* WeightText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* SlotsText;

	// Gold display
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* GoldText;

	// Tooltip widget
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TSubclassOf<UItemTooltipWidget> ItemTooltipWidgetClass;

	UPROPERTY()
	UItemTooltipWidget* ItemTooltipWidget;

	// Context menu widget class
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TSubclassOf<UItemActionMenuWidget> ItemContextMenuWidgetClass;

	UPROPERTY()
	UItemActionMenuWidget* ItemContextMenuWidget;

	// Drop quantity popup widget class
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TSubclassOf<UDropQuantityPopupWidget> DropQuantityPopupWidgetClass;

	UPROPERTY()
	UDropQuantityPopupWidget* DropQuantityPopupWidget;

	// Inventory slot widget class
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	TSubclassOf<UInventorySlotWidget> InventorySlotWidgetClass;

	// Current inventory data
	UPROPERTY(BlueprintReadOnly, Category = "Inventory UI")
	FCharacterInventoryStruct CurrentInventory;

	// Inventory manager reference
	UPROPERTY()
	UInventoryManager* InventoryManager;

	// Inventory settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	int32 InventoryRows = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	int32 InventoryColumns = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	FVector2D SlotSize = FVector2D(64.0f, 64.0f);

	// Weight limit for inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory UI")
	float MaxInventoryWeight = 100.0f;

	// Inventory slots
	UPROPERTY()
	TArray<UInventorySlotWidget*> InventorySlots;

	// UI state
	bool bIsInventoryVisible = false;
	int32 HoveredSlotIndex = -1;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* DragHandle = nullptr; // ������� ������/����� ���� (����� ����� ������)

	bool bDragging = false;
	FVector2D DragOffset = FVector2D::ZeroVector;
	FMargin DragPadding = FMargin(0.f); // ��������� ������ �� �����

	UPROPERTY()
	FVector2D CurrentViewportPosition = FVector2D::ZeroVector;

	bool bHasSwitchedToAbsolute = false;

private:
	// Helper functions
	int32 GetSlotIndexFromPosition(int32 Row, int32 Column) const;
	void GetPositionFromSlotIndex(int32 SlotIndex, int32& OutRow, int32& OutColumn) const;
	int32 GetTotalSlots() const { return InventoryRows * InventoryColumns; }

	// Handle close button click
	UFUNCTION()
	void OnCloseButtonClicked();
};