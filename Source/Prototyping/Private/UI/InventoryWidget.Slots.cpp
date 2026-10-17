#include "UI/InventoryWidget.h"
#include "UI/InventorySlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "UI/ItemActionMenuWidget.h"
#include "UI/DropQuantityPopupWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Components/GridPanel.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

void UInventoryWidget::OnSlotClicked(int32 SlotIndex)
{
	// Close context menu when clicking on any slot
	CloseContextMenu();

	// Hide tooltip when slot is clicked
	HideTooltip();
	
	UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (SlotWidget && SlotWidget->HasItem())
	{
		FInventoryItemStruct Item = SlotWidget->GetItemData();
		OnInventorySlotClicked.Broadcast(SlotIndex, Item);

		UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Slot %d clicked - Item: %s"), SlotIndex, *Item.name);
	}
}

void UInventoryWidget::OnSlotRightClicked(int32 SlotIndex)
{
	// Always hide the hover tooltip first
	HideTooltip();

	UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (!SlotWidget || !SlotWidget->HasItem()) return;

	const FInventoryItemStruct& Item = SlotWidget->GetItemData();

	// Get current cursor screen position
	FVector2D MousePos = FVector2D::ZeroVector;
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->GetMousePosition(MousePos.X, MousePos.Y);
	}

	OpenContextMenu(Item, MousePos);

	// Also broadcast so external listeners can react if needed
	OnInventorySlotRightClicked.Broadcast(SlotIndex, Item);

	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Context menu opened for slot %d � %s"), SlotIndex, *Item.name);
}

void UInventoryWidget::OnSlotHovered(int32 SlotIndex, bool bIsHovered)
{
	if (bIsHovered)
	{
		HoveredSlotIndex = SlotIndex;
		UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
		if (SlotWidget && SlotWidget->HasItem())
		{
			// Get mouse position for tooltip
			FVector2D MousePosition = FVector2D::ZeroVector;
			if (GetWorld() && GetWorld()->GetFirstPlayerController())
			{
				GetWorld()->GetFirstPlayerController()->GetMousePosition(MousePosition.X, MousePosition.Y);
			}

			ShowTooltip(SlotWidget->GetItemData(), MousePosition);
		}
	}
	else
	{
		if (HoveredSlotIndex == SlotIndex)
		{
			HoveredSlotIndex = -1;
			HideTooltip();
		}
	}
}

void UInventoryWidget::OnInventoryUpdated(const FCharacterInventoryStruct& UpdatedInventory)
{
	UpdateInventoryDisplay(UpdatedInventory);
}

void UInventoryWidget::OnItemAdded(const FInventoryItemStruct& Item, int32 Quantity)
{
	// Visual feedback for item addition could be added here
	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Item added - %s x%d"), *Item.name, Quantity);
}

void UInventoryWidget::OnItemRemoved(const FInventoryItemStruct& Item, int32 Quantity)
{
	// Visual feedback for item removal could be added here
	UE_LOG(LogTemp, Log, TEXT("InventoryWidget: Item removed - %s x%d"), *Item.name, Quantity);
}

