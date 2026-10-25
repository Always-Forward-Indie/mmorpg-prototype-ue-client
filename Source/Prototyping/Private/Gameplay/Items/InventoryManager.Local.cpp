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

void UInventoryManager::ResolveUnresolvedRows()
{
	if (!gameInstance) return;
	UItemManager* ItemMgr = gameInstance->GetItemManager();
	if (!ItemMgr) return;
	for (FInventoryItemStruct& Row : CurrentInventory.items)
	{
		if (!Row.slug.IsEmpty() || Row.itemId <= 0) continue;
		FInventoryItemStruct Stats;
		if (!ItemMgr->GetCatalogStats(Row.itemId, Stats)) continue;
		// Instance state (id/qty/durability/equipped/slot/kill) is authoritative
		// from the delta — only statics come from the catalog.
		const int32 KeepId = Row.id;
		const int32 KeepQty = Row.quantity;
		const int32 KeepDur = Row.durabilityCurrent;
		const bool KeepEquipped = Row.is_equipped;
		const int32 KeepSlot = Row.slotIndex;
		const int32 KeepKill = Row.killCount;
		Row = Stats;
		Row.id = KeepId;
		Row.quantity = KeepQty;
		Row.durabilityCurrent = KeepDur;
		Row.is_equipped = KeepEquipped;
		Row.slotIndex = KeepSlot;
		Row.killCount = KeepKill;
		Row.itemId = Stats.itemId;
	}
}

void UInventoryManager::UpdateLocalInventory(const FCharacterInventoryStruct& NewInventory)
{
	FCharacterInventoryStruct OldInventory = CurrentInventory;
	CurrentInventory = NewInventory;

	// Compare old and new inventory to detect changes
	for (const FInventoryItemStruct& NewItem : NewInventory.items)
	{
		FInventoryItemStruct* OldItem = OldInventory.items.FindByPredicate(
			[NewItem](const FInventoryItemStruct& Item) { return (NewItem.id > 0 && Item.id > 0) ? (Item.id == NewItem.id) : (Item.itemId == NewItem.itemId); });

		if (OldItem)
		{
			// Item existed before
			if (OldItem->quantity != NewItem.quantity)
			{
				if (NewItem.quantity > OldItem->quantity)
				{
					// Item quantity increased
					OnItemAdded.Broadcast(NewItem, NewItem.quantity - OldItem->quantity);
				}
				else
				{
					// Item quantity decreased
					OnItemRemoved.Broadcast(NewItem, OldItem->quantity - NewItem.quantity);
				}
			}
		}
		else
		{
			// New item added
			OnItemAdded.Broadcast(NewItem, NewItem.quantity);
		}
	}

	// Check for completely removed items
	for (const FInventoryItemStruct& OldItem : OldInventory.items)
	{
		bool bFoundInNew = NewInventory.items.ContainsByPredicate(
			[OldItem](const FInventoryItemStruct& Item) { return (OldItem.id > 0 && Item.id > 0) ? (Item.id == OldItem.id) : (Item.itemId == OldItem.itemId); });

		if (!bFoundInNew)
		{
			// Item was completely removed
			OnItemRemoved.Broadcast(OldItem, OldItem.quantity);
		}
	}

	OnInventoryUpdated.Broadcast(CurrentInventory);
	UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Inventory updated for character %d"), CurrentInventory.characterId);
}

void UInventoryManager::RequestInventoryData(int32 CharacterId)
{
	if (!networkManager)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryManager: Network manager not found"));
		OnInventoryLoadComplete.Broadcast(false);
		return;
	}

	if (!gameInstance)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryManager: Game instance not found"));
		OnInventoryLoadComplete.Broadcast(false);
		return;
	}

	// DevMode offline: inventory comes from DevModeDataProvider, no chunk to
	// ask — skip quietly instead of Erroring on the missing worker.
	if (gameInstance->DevModeConfig.bEnabled)
	{
		UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: skipping inventory request (DevMode offline)"));
		return;
	}

	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;
	
	// Add client ID and hash from the game instance
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID()));
	HeaderData.Add(TEXT("clientId"), ClientIDValue);
	
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(gameInstance->GetCurrentClientHash()));
	HeaderData.Add(TEXT("hash"), HashValue);
	
	// Add the character ID to the body data
	TSharedPtr<FJsonValueNumber> CharacterIDValue = MakeShareable(new FJsonValueNumber(CharacterId));
	BodyData.Add(TEXT("characterId"), CharacterIDValue);
	
	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("getPlayerInventory"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Sent inventory request for character: %d"), CharacterId);
}

