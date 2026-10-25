#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Utils/NetCore/InventoryRules.h"
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

void UInventoryManager::ProcessGameServerData(const FString& ReceivedData)
{
	CRASH_GUARD("InventoryManager::ProcessGameServerData");
	if (!gameInstance) return;

	// Process time sync data first
	//if (gameInstance && gameInstance->GetTimeSyncService())
	//{
	//	JSONParser::ProcessTimeSyncFromHeader(ReceivedData, gameInstance->GetTimeSyncService());
	//}

	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Received event type: %s"), *MessageData.eventType);

	// Handle inventory data response
	if (MessageData.eventType == "getPlayerInventory" && MessageData.status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Received inventory data"));
		ProcessInventoryData(ReceivedData);
	}
	// Handle inventory update events
	else if (MessageData.eventType == "INVENTORY_UPDATE" && MessageData.status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Received inventory update"));
		ProcessInventoryUpdate(ReceivedData);
	}
	// Handle use item response
	else if (MessageData.eventType == "useItem" && MessageData.status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Item used successfully"));
		ProcessInventoryUpdate(ReceivedData);

		int32 UsedItemId = 0;
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			if (TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body")))
			{
				UsedItemId = Body->GetIntegerField(TEXT("itemId"));
			}
		}

		if (UsedItemId > 0 && gameInstance)
		{
			FInventoryItemStruct Item = GetItemById(UsedItemId);
			if (!Item.itemSlug.IsEmpty())
			{
				FItemVisualData Vis = gameInstance->GetItemManager()->GetItemVisualDataBySlug(Item.itemSlug);
				if (USoundBase* Snd = Vis.UseSound.LoadSynchronous())
				{
					USoundAttenuation* Atn = Vis.DefaultAttenuation.LoadSynchronous();
					ABasicPlayer* Player = gameInstance->GetPlayerByCharacterId(gameInstance->GetCurrentCharacterID());
					if (Player)
						SpawnSFXAttached(Player, Snd, Player->GetActorLocation(), 1.0f, Atn);
				}
				else if (gameInstance->AudioManager)
				{
					gameInstance->AudioManager->PlayUISound(EUISoundEvent::ItemUse);
				}
			}
		}
	}
	// Handle drop item response
	else if (MessageData.eventType == "dropItem" && MessageData.status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Item dropped successfully"));
		ProcessInventoryUpdate(ReceivedData);
	}
	// Handle weapon kill count update (item soul system — server pushes new count on every kill)
	else if (MessageData.eventType == "world_notification")
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
			if (Root->TryGetObjectField(TEXT("body"), BodyPtr) && (*BodyPtr).IsValid())
			{
				FString NotifType;
				(*BodyPtr)->TryGetStringField(TEXT("notificationType"), NotifType);
				if (NotifType == TEXT("weapon_kill_count_update"))
				{
					const TSharedPtr<FJsonObject>* DataPtr = nullptr;
					if ((*BodyPtr)->TryGetObjectField(TEXT("data"), DataPtr) && (*DataPtr).IsValid())
					{
						int32 InventoryItemId = (*DataPtr)->GetIntegerField(TEXT("inventoryItemId"));
						int32 NewKillCount    = (*DataPtr)->GetIntegerField(TEXT("killCount"));
						if (InventoryItemId > 0)
						{
							for (FInventoryItemStruct& Item : CurrentInventory.items)
							{
								if (Item.id == InventoryItemId)
								{
									Item.killCount = NewKillCount;
									UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: Updated killCount=%d for inventoryItemId=%d"), NewKillCount, InventoryItemId);
									OnInventoryUpdated.Broadcast(CurrentInventory);
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}

void UInventoryManager::ProcessInventoryData(const FString& JsonData)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
	
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("InventoryManager: Failed to deserialize inventory data"));
		OnInventoryLoadComplete.Broadcast(false);
		return;
	}

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryManager: No body found in inventory data"));
		OnInventoryLoadComplete.Broadcast(false);
		return;
	}


	// Use the shared parser that handles nested "item" sub-object per protocol
	FCharacterInventoryStruct Parsed = JSONParser::DeserializeCharacterInventory(Body);

	// Reject packets belonging to a different character
	if (OwnerCharacterId > 0 && Parsed.characterId > 0 && Parsed.characterId != OwnerCharacterId)
	{
		UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: Ignoring inventory data for CharID=%d (owner=%d)"),
			Parsed.characterId, OwnerCharacterId);
		return;
	}

	CurrentInventory = Parsed;

	// Apply is_equipped flags from EquipmentManager if it's already loaded
	if (gameInstance)
	{
		if (UEquipmentManager* EquipMgr = gameInstance->GetEquipmentManager())
		{
			const FEquipmentStateData& EquipState = EquipMgr->GetEquipmentState();
			for (const auto& SlotPair : EquipState.slots)
			{
				const FEquipmentSlotData& Slot = SlotPair.Value;
				if (!Slot.bIsOccupied || Slot.inventoryItemId <= 0) continue;
				for (FInventoryItemStruct& Item : CurrentInventory.items)
				{
					if (Item.id == Slot.inventoryItemId)
					{
						Item.is_equipped = true;
						break;
					}
				}
			}
		}
	}

	bInventoryLoaded = true;
	UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Loaded %d items, gold=%d for character %d"),
		CurrentInventory.items.Num(), CurrentInventory.gold, CurrentInventory.characterId);

	// Broadcast events
	OnInventoryUpdated.Broadcast(CurrentInventory);
	OnInventoryLoadComplete.Broadcast(true);
}

void UInventoryManager::ProcessInventoryUpdate(const FString& JsonData)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
	
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("InventoryManager: Failed to deserialize inventory update"));
		return;
	}

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryManager: No body found in inventory update"));
		return;
	}

	// S2.3 delta shape: body = {characterId, changed[], removed[], gold}.
	// Slim rows merge into the cached inventory (statics resolve from the
	// itemCatalog); full loads still arrive via ProcessInventoryData.
	const TArray<TSharedPtr<FJsonValue>>* ChangedArray = nullptr;
	if (Body->TryGetArrayField(TEXT("changed"), ChangedArray))
	{
		int32 CharacterId = 0;
		Body->TryGetNumberField(TEXT("characterId"), CharacterId);
		if (OwnerCharacterId > 0 && CharacterId > 0 && CharacterId != OwnerCharacterId)
		{
			UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: Ignoring INVENTORY_UPDATE for CharID=%d (owner=%d)"), CharacterId, OwnerCharacterId);
			return;
		}

		TArray<FInventoryItemStruct> Changed;
		for (const TSharedPtr<FJsonValue>& ItemValue : *ChangedArray)
		{
			const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
			if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemPtr) || !ItemPtr || !(*ItemPtr).IsValid()) continue;
			Changed.Add(JSONParser::DeserializeInventoryItem(*ItemPtr));
		}
		TArray<int32> Removed;
		const TArray<TSharedPtr<FJsonValue>>* RemovedArray = nullptr;
		if (Body->TryGetArrayField(TEXT("removed"), RemovedArray))
		{
			for (const TSharedPtr<FJsonValue>& RemValue : *RemovedArray)
			{
				if (RemValue.IsValid())
					Removed.Add(static_cast<int32>(RemValue->AsNumber()));
			}
		}

		if (CharacterId > 0)
			CurrentInventory.characterId = CharacterId;
		int32 Gold = CurrentInventory.gold;
		Body->TryGetNumberField(TEXT("gold"), Gold);
		CurrentInventory.gold = Gold;

		FInventoryRules::ApplyInventoryDelta(CurrentInventory.items, Changed, Removed);
		ResolveUnresolvedRows();
		OnInventoryUpdated.Broadcast(CurrentInventory);
		return;
	}

	// Check if this contains a data object with inventory update
	TSharedPtr<FJsonObject> DataObject = Body->GetObjectField(TEXT("data"));
	if (DataObject.IsValid())
	{
		// This is an INVENTORY_UPDATE format
		FCharacterInventoryStruct NewInventory;
		
		int32 CharacterId = 0;
		DataObject->TryGetNumberField(TEXT("characterId"), CharacterId);
		NewInventory.characterId = CharacterId;

		// Filter: ignore updates that belong to a different character.
		if (OwnerCharacterId > 0 && CharacterId > 0 && CharacterId != OwnerCharacterId)
		{
			UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: Ignoring INVENTORY_UPDATE for CharID=%d (owner=%d)"), CharacterId, OwnerCharacterId);
			return;
		}

		// Parse simplified items array (only itemId and quantity)
		const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
		if (DataObject->TryGetArrayField(TEXT("items"), ItemsArray))
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *ItemsArray)
			{
				// TryGetObject (not AsObject): wrong-typed entries skip quietly.
				const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
				if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemPtr) || !ItemPtr || !(*ItemPtr).IsValid())
				{
					continue;
				}
				const TSharedPtr<FJsonObject>& ItemObject = *ItemPtr;
				int32 ItemId = 0;
				int32 Quantity = 0;
				
				ItemObject->TryGetNumberField(TEXT("itemId"), ItemId);
				ItemObject->TryGetNumberField(TEXT("quantity"), Quantity);

				// Read the unique instance id — process each server row independently
				// to avoid duplicating ids when multiple items share the same itemId.
				int32 InstanceId = 0;
				ItemObject->TryGetNumberField(TEXT("id"), InstanceId);

				// Find existing item by unique instance id (not template itemId)
				FInventoryItemStruct* ExistingItem = nullptr;
				if (InstanceId > 0)
				{
					ExistingItem = CurrentInventory.items.FindByPredicate(
						[InstanceId](const FInventoryItemStruct& Item) { return Item.id == InstanceId; });
				}
				else
				{
					// Fallback: no id field — match by itemId (legacy/partial updates)
					ExistingItem = CurrentInventory.items.FindByPredicate(
						[ItemId](const FInventoryItemStruct& Item) { return Item.itemId == ItemId; });
				}

				if (ExistingItem)
				{
					// Update quantity of existing item
					ExistingItem->quantity = Quantity;
					NewInventory.items.Add(*ExistingItem);
				}
				else
				{
					// This is a new item, create with minimal data
					FInventoryItemStruct NewItem;
					NewItem.itemId = ItemId;
					NewItem.quantity = Quantity;
					NewItem.name = FString::Printf(TEXT("Item %d"), ItemId); // Placeholder
					NewInventory.items.Add(NewItem);
					
						UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: New item detected (ID: %d), may need full item data"), ItemId);
					}
			}
		}

		UpdateLocalInventory(NewInventory);
	}
	else
	{
		// This might be a direct inventory update, process as full inventory data
		ProcessInventoryData(JsonData);
	}
}

