#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "UI/InventoryWidget.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Audio/AudioManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "CrashDiagnostics.h"

void UInventoryManager::SetInventoryUIWidget(UInventoryWidget* InInventoryUIWidget)
{
	if (InInventoryUIWidget)
	{
		this->InventoryUIWidget = InInventoryUIWidget;
		InInventoryUIWidget->InitializeInventory(this);

		// Bind UI events
		InInventoryUIWidget->OnInventorySlotClicked.AddDynamic(this, &UInventoryManager::HandleSlotClicked);
		// OnInventorySlotRightClicked is handled entirely by ItemActionMenuWidget - do NOT bind HandleSlotRightClicked here.
		
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: UI Widget set and initialized"));
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryManager: Cannot set null UI Widget"));
	}
}

void UInventoryManager::ShowInventoryUI()
{
	if (InventoryUIWidget)
	{
		InventoryUIWidget->SetInventoryVisible(true);
	}
}

void UInventoryManager::HideInventoryUI()
{
	if (InventoryUIWidget)
	{
		InventoryUIWidget->SetInventoryVisible(false);
	}
}

void UInventoryManager::ToggleInventoryUI()
{
	if (InventoryUIWidget)
	{
		InventoryUIWidget->ToggleInventory();
	}
}

bool UInventoryManager::IsInventoryUIVisible() const
{
	if (InventoryUIWidget)
	{
		return InventoryUIWidget->IsInventoryVisible();
	}
	return false;
}

void UInventoryManager::HandleSlotClicked(int32 SlotIndex, const FInventoryItemStruct& Item)
{
	// All item actions are handled via the context menu (ItemActionMenuWidget). No action on plain LMB click.
	UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: Slot %d clicked - handled by context menu"), SlotIndex);
}

void UInventoryManager::HandleSlotRightClicked(int32 SlotIndex, const FInventoryItemStruct& Item)
{
	// Right-click is handled by the context menu (ItemActionMenuWidget). No action here.
	UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: Slot %d right-clicked - handled by context menu"), SlotIndex);
}

void UInventoryManager::AddItemToLocalInventory(const FInventoryItemStruct& Item)
{
	// Find existing item by unique instance ID (id); fall back to template itemId only for legacy rows without id
	FInventoryItemStruct* ExistingItem = CurrentInventory.items.FindByPredicate(
		[Item](const FInventoryItemStruct& InventoryItem) { return (Item.id > 0 && InventoryItem.id > 0) ? (InventoryItem.id == Item.id) : (InventoryItem.itemId == Item.itemId); });

	if (ExistingItem)
	{
		// Item already exists, add to quantity
		int32 OldQuantity = ExistingItem->quantity;
		ExistingItem->quantity += Item.quantity;
		
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Updated item %s quantity from %d to %d"), 
			*Item.name, OldQuantity, ExistingItem->quantity);
		
		OnItemAdded.Broadcast(*ExistingItem, Item.quantity);
	}
	else
	{
		// New item, add to inventory
		CurrentInventory.items.Add(Item);
		
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Added new item %s (ID: %d, Quantity: %d)"), 
			*Item.name, Item.itemId, Item.quantity);
		
		OnItemAdded.Broadcast(Item, Item.quantity);
	}

	// Broadcast inventory update
	OnInventoryUpdated.Broadcast(CurrentInventory);
}
