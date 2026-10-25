#include "UI/InventoryWidget.h"
#include "Prototyping.h"
#include "UI/InventorySlotWidget.h"
#include "Utils/NetCore/SlotMath.h"
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

void UInventoryWidget::OpenContextMenu(const FInventoryItemStruct& Item, FVector2D ScreenPosition)
{
	if (!ItemContextMenuWidget) return;

	// Close drop popup if open
	if (DropQuantityPopupWidget)
		DropQuantityPopupWidget->HidePopup();

	// Inject manager references so the menu can dispatch requests directly
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	ItemContextMenuWidget->SetManagers(
		InventoryManager,
		GI ? GI->GetEquipmentManager() : nullptr);

	ItemContextMenuWidget->ShowForItem(Item, ScreenPosition);
}

void UInventoryWidget::CloseContextMenu()
{
	if (ItemContextMenuWidget)
		ItemContextMenuWidget->HideMenu();
}

void UInventoryWidget::HandleContextAction(EItemContextAction Action, const FInventoryItemStruct& Item)
{
	// Drop is the only action that needs a UI step (quantity picker).
	// All other actions are already dispatched inside ItemActionMenuWidget::ExecuteAction.
	if (Action != EItemContextAction::Drop) return;

	if (Item.quantity <= 1)
	{
		// Single item � drop immediately without asking
		HandleDropConfirmed(Item, 1);
		return;
	}

	// Stacked item � show quantity picker
	if (DropQuantityPopupWidget)
	{
		DropQuantityPopupWidget->ShowForItem(Item);
	}
	else
	{
		// No popup configured � drop everything
		HandleDropConfirmed(Item, Item.quantity);
	}
}

void UInventoryWidget::HandleDropConfirmed(const FInventoryItemStruct& Item, int32 Quantity)
{
	if (!InventoryManager) return;

	InventoryManager->DropItem(Item.itemId, Quantity);

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Drop confirmed � itemId=%d qty=%d"), Item.itemId, Quantity);
}

void UInventoryWidget::HandleDropCancelled()
{
	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Drop cancelled by player."));
}

void UInventoryWidget::ShowTooltip(const FInventoryItemStruct& Item, FVector2D Position)
{
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->SetItemData(Item);
		ItemTooltipWidget->UpdateTooltipPosition(Position);
		ItemTooltipWidget->ShowTooltip();
	}
}

void UInventoryWidget::HideTooltip()
{
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->HideTooltip();
	}
}

int32 UInventoryWidget::GetSlotIndexFromPosition(int32 Row, int32 Column) const
{
	// Mapping lives in FSlotMath (pinned by MMO.Inventory.SlotMath).
	return FSlotMath::IndexFromPosition(Row, Column, InventoryColumns);
}

void UInventoryWidget::GetPositionFromSlotIndex(int32 SlotIndex, int32& OutRow, int32& OutColumn) const
{
	// Mapping lives in FSlotMath (pinned by MMO.Inventory.SlotMath).
	FSlotMath::PositionFromIndex(SlotIndex, InventoryColumns, OutRow, OutColumn);
}

void UInventoryWidget::OnCloseButtonClicked()
{
	SetInventoryVisible(false);
}

