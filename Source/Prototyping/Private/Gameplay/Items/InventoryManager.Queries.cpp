#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
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

bool UInventoryManager::HasItem(int32 ItemId) const
{
	return CurrentInventory.items.ContainsByPredicate(
		[ItemId](const FInventoryItemStruct& Item) { return Item.itemId == ItemId; });
}

FInventoryItemStruct UInventoryManager::GetItemById(int32 ItemId) const
{
	const FInventoryItemStruct* FoundItem = CurrentInventory.items.FindByPredicate(
		[ItemId](const FInventoryItemStruct& Item) { return Item.itemId == ItemId; });

	return FoundItem ? *FoundItem : FInventoryItemStruct();
}

int32 UInventoryManager::GetItemQuantity(int32 ItemId) const
{
	const FInventoryItemStruct* FoundItem = CurrentInventory.items.FindByPredicate(
		[ItemId](const FInventoryItemStruct& Item) { return Item.itemId == ItemId; });

	return FoundItem ? FoundItem->quantity : 0;
}

TArray<FInventoryItemStruct> UInventoryManager::GetItemsByType(const FString& ItemType) const
{
	TArray<FInventoryItemStruct> ItemsOfType;
	
	for (const FInventoryItemStruct& Item : CurrentInventory.items)
	{
		if (Item.type.Equals(ItemType, ESearchCase::IgnoreCase))
		{
			ItemsOfType.Add(Item);
		}
	}
	
	return ItemsOfType;
}

TArray<FInventoryItemStruct> UInventoryManager::GetItemsByRarity(const FString& Rarity) const
{
	TArray<FInventoryItemStruct> ItemsOfRarity;
	
	for (const FInventoryItemStruct& Item : CurrentInventory.items)
	{
		if (Item.rarity.Equals(Rarity, ESearchCase::IgnoreCase))
		{
			ItemsOfRarity.Add(Item);
		}
	}
	
	return ItemsOfRarity;
}

int32 UInventoryManager::GetTotalItemCount() const
{
	int32 TotalCount = 0;
	for (const FInventoryItemStruct& Item : CurrentInventory.items)
	{
		TotalCount += Item.quantity;
	}
	return TotalCount;
}

