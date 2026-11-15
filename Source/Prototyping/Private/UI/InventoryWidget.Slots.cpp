#include "UI/InventoryWidget.h"
#include "Prototyping.h"
#include "UI/InventorySlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "UI/ItemActionMenuWidget.h"
#include "UI/DropQuantityPopupWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "MyGameInstance.h"
#include "Utils/NetCore/InventorySelectionRule.h"
#include "HAL/PlatformTime.h"
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
	if (!SlotWidget)
	{
		return;
	}

	// Double-click: second LMB on the already-selected slot inside the
	// window acts (Use/Equip) instead of deselecting. The chain resets so
	// a third fast click starts fresh as a single (never an accidental
	// second activation on a consumable).
	const double Now = FPlatformTime::Seconds();
	if (SlotWidget->HasItem() && SlotIndex == SelectedSlotIndex &&
		FInventorySelectionRule::IsDoubleClick(SelectedSlotIndex, LastClickSlot, LastClickTime, Now, DoubleClickWindow))
	{
		HandleDoubleClick(SlotWidget->GetItemData());
		LastClickSlot = INDEX_NONE;
		LastClickTime = 0.0;
		return;
	}

	LastClickSlot = SlotIndex;
	LastClickTime = Now;

	// Empty cell click clears a stale highlight (never selects emptiness).
	const bool bHasItem = SlotWidget->HasItem();
	const int32 NextSelected = FInventorySelectionRule::Resolve(SelectedSlotIndex, SlotIndex, bHasItem);

	for (int32 i = 0; i < InventorySlots.Num(); ++i)
	{
		if (UInventorySlotWidget* Other = InventorySlots[i])
		{
			Other->SetSlotSelected(i == NextSelected);
		}
	}

	SelectedSlotIndex = NextSelected;
	SelectedInstanceId = (NextSelected != INDEX_NONE && GetSlotWidget(NextSelected))
		? GetSlotWidget(NextSelected)->GetItemData().id
		: 0;

	if (NextSelected != INDEX_NONE && SlotWidget->HasItem())
	{
		FInventoryItemStruct Item = SlotWidget->GetItemData();
		OnInventorySlotClicked.Broadcast(SlotIndex, Item);

		UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Slot %d selected - Item: %s"), SlotIndex, *Item.name);
	}
	else
	{
		UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Selection cleared (clicked %d)"), SlotIndex);
	}
}

void UInventoryWidget::ClearSelection()
{
	SelectedSlotIndex = INDEX_NONE;
	SelectedInstanceId = 0;
	for (UInventorySlotWidget* SlotWidget : InventorySlots)
	{
		if (SlotWidget)
		{
			SlotWidget->SetSlotSelected(false);
		}
	}
}

void UInventoryWidget::HandleDoubleClick(const FInventoryItemStruct& Item)
{
	// Usable wins on hybrid rows (menu offers both; use is the WoW default).
	if (Item.isUsable)
	{
		if (!InventoryManager) return;
		UE_LOG(LogGameplay, Log, TEXT("InventoryWidget: Double-click Use - %s (template %d)"), *Item.name, Item.itemId);
		InventoryManager->UseItem(Item.itemId);
		return;
	}

	if (Item.isEquippable)
	{
		UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
		UEquipmentManager* EquipmentManager = GI ? GI->GetEquipmentManager() : nullptr;
		const int32 CharacterId = InventoryManager ? InventoryManager->GetInventory().characterId : 0;
		if (!EquipmentManager || CharacterId <= 0)
		{
			UE_LOG(LogGameplay, Warning, TEXT("InventoryWidget: Double-click equip unavailable (manager=%p char=%d)"), EquipmentManager, CharacterId);
			return;
		}
		if (Item.is_equipped)
		{
			// Mirror of the menu guard: no slot slug, no request.
			if (Item.equipSlotSlug.IsEmpty()) return;
			UE_LOG(LogGameplay, Log, TEXT("InventoryWidget: Double-click Unequip - %s"), *Item.name);
			EquipmentManager->RequestUnequipItem(CharacterId, Item.equipSlotSlug);
		}
		else
		{
			// id = player_inventory PK (inventoryItemId per protocol),
			// template fallback mirrors the menu for pre-token rows.
			const int32 InvItemId = (Item.id > 0) ? Item.id : Item.itemId;
			UE_LOG(LogGameplay, Log, TEXT("InventoryWidget: Double-click Equip - %s"), *Item.name);
			EquipmentManager->RequestEquipItem(CharacterId, InvItemId);
		}
		return;
	}

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Double-click on %s - not usable/equippable, no-op"), *Item.name);
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

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Context menu opened for slot %d � %s"), SlotIndex, *Item.name);
}

void UInventoryWidget::OnSlotHovered(int32 SlotIndex, bool bIsHovered)
{
	if (bIsHovered)
	{
		HoveredSlotIndex = SlotIndex;
		UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
		if (SlotWidget && SlotWidget->HasItem())
		{
			// Seen = no longer new (session badge, cleared once).
			const int32 InstanceId = SlotWidget->GetItemData().id;
			if (InstanceId > 0 && NewInstanceIds.Remove(InstanceId) > 0)
			{
				SlotWidget->SetNewBadge(false);
				UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: New badge cleared (instance %d)"), InstanceId);
			}

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
	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Item added - %s x%d"), *Item.name, Quantity);
}

void UInventoryWidget::OnItemRemoved(const FInventoryItemStruct& Item, int32 Quantity)
{
	// Visual feedback for item removal could be added here
	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Item removed - %s x%d"), *Item.name, Quantity);
}

void UInventoryWidget::HandleItemCooldown(int32 ItemId, int32 CooldownSeconds)
{
	if (ItemId <= 0 || CooldownSeconds <= 0) return;
	for (UInventorySlotWidget* SlotWidget : InventorySlots)
	{
		if (SlotWidget && SlotWidget->HasItem() && SlotWidget->GetItemData().itemId == ItemId)
		{
			SlotWidget->StartCooldown(static_cast<float>(CooldownSeconds));
		}
	}
}

