#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/InventoryQuery.h"
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
	// Lookup lives in FInventoryQuery (pinned by MMO.Inventory.Query).
	return FInventoryQuery::HasItem(CurrentInventory.items, ItemId);
}

FInventoryItemStruct UInventoryManager::GetItemById(int32 ItemId) const
{
	// Lookup lives in FInventoryQuery (pinned by MMO.Inventory.Query).
	const int32 Idx = FInventoryQuery::IndexOf(CurrentInventory.items, ItemId);
	return Idx != INDEX_NONE ? CurrentInventory.items[Idx] : FInventoryItemStruct();
}

int32 UInventoryManager::GetItemQuantity(int32 ItemId) const
{
	// Lookup lives in FInventoryQuery (pinned by MMO.Inventory.Query).
	return FInventoryQuery::QuantityOf(CurrentInventory.items, ItemId);
}

TArray<FInventoryItemStruct> UInventoryManager::GetItemsByType(const FString& ItemType) const
{
	// Filter lives in FInventoryQuery (pinned by MMO.Inventory.Query).
	return FInventoryQuery::ByType(CurrentInventory.items, ItemType);
}

TArray<FInventoryItemStruct> UInventoryManager::GetItemsByRarity(const FString& Rarity) const
{
	// Filter lives in FInventoryQuery (pinned by MMO.Inventory.Query).
	return FInventoryQuery::ByRarity(CurrentInventory.items, Rarity);
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

