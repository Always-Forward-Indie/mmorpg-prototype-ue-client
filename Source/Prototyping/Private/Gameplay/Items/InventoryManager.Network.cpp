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

void UInventoryManager::HandleChunkPacket(const FParsedPacket& Packet)
{
	CRASH_GUARD("InventoryManager::HandleChunkPacket");
	if (!gameInstance) return;

	// Single-parse path (S05): EventType/Status/Body come from PollLink, no re-parse.
	if (!Packet.bValid || !Packet.Root.IsValid()) return;

	// Compat: ProcessInventoryData/ProcessInventoryUpdate still take the raw string (S05b converts them).
	const FString& ReceivedData = Packet.Raw;

	const FString& EventType = Packet.EventType;
	const FString& Status = Packet.Status;
	const TSharedPtr<FJsonObject>& Body = Packet.Body;

	// Time sync is handled by the dispatcher fallback.

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: Received event type: %s"), *EventType);

	// Handle inventory data response
	if (EventType == "getPlayerInventory" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Received inventory data"));
		ProcessInventoryData(ReceivedData);
	}
	// Handle inventory update events
	else if (EventType == "INVENTORY_UPDATE" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Received inventory update"));
		ProcessInventoryUpdate(ReceivedData);
	}
	// Handle use item response
	else if (EventType == "useItem" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Item used successfully"));
		ProcessInventoryUpdate(ReceivedData);

		int32 UsedItemId = 0;
		if (Body.IsValid())
		{
			UsedItemId = Body->GetIntegerField(TEXT("itemId"));
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
	else if (EventType == "dropItem" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Item dropped successfully"));
		ProcessInventoryUpdate(ReceivedData);
	}
	// Handle weapon kill count update (item soul system — server pushes new count on every kill)
	else if (EventType == "world_notification")
	{
		if (Body.IsValid())
		{
			FString NotifType;
			Body->TryGetStringField(TEXT("notificationType"), NotifType);
			if (NotifType == TEXT("weapon_kill_count_update"))
			{
				const TSharedPtr<FJsonObject>* DataPtr = nullptr;
				if (Body->TryGetObjectField(TEXT("data"), DataPtr) && (*DataPtr).IsValid())
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
	bool bFullResync = false;
	Body->TryGetBoolField(TEXT("fullResync"), bFullResync);
	UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Loaded %d items, gold=%d version=%lld for character %d%s"),
		CurrentInventory.items.Num(), CurrentInventory.gold, CurrentInventory.inventoryVersion, CurrentInventory.characterId,
		bFullResync ? TEXT(" (fullResync)") : TEXT(""));

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

		// S15a generation gate (pure rule, NetCore): stale duplicates drop
		// silently, gaps trigger an explicit full refresh instead of a
		// half-applied merge. Snapshots bypass this (authoritative).
		int64 IncomingVersion = 0;
		double VersionNumber = 0;
		if (Body->TryGetNumberField(TEXT("version"), VersionNumber))
		{
			IncomingVersion = static_cast<int64>(FMath::RoundToDouble(VersionNumber));
		}
		const EInventoryVersionVerdict Verdict = FInventoryVersionRule::Classify(CurrentInventory.inventoryVersion, IncomingVersion);
		if (Verdict == EInventoryVersionVerdict::Stale)
		{
			UE_LOG(LogNetPacket, Verbose, TEXT("InventoryManager: Ignoring stale INVENTORY_UPDATE v%lld (current v%lld)"),
				IncomingVersion, CurrentInventory.inventoryVersion);
			return;
		}
		if (Verdict == EInventoryVersionVerdict::Gap)
		{
			UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: INVENTORY_UPDATE v%lld skips v%lld — requesting full refresh"),
				IncomingVersion, CurrentInventory.inventoryVersion + 1);
			RequestInventoryData(OwnerCharacterId > 0 ? OwnerCharacterId : CharacterId);
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
		// S15a: baseline advances only on accepted deltas (gap path returns
		// early above; pre-token version-0 deltas keep baseline 0).
		CurrentInventory.inventoryVersion = IncomingVersion;
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
					// Fallback: no id field — match by itemId (pre-token/partial updates)
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

