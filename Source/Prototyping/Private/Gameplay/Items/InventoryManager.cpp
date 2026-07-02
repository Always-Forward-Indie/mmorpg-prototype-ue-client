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

UInventoryManager::UInventoryManager()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Initialize references
	networkManager = nullptr;
	gameInstance = nullptr;
	worldContext = nullptr;
	bInventoryLoaded = false;

	// Initialize inventory
	CurrentInventory = FCharacterInventoryStruct();
}

void UInventoryManager::BeginPlay()
{
	Super::BeginPlay();
}

void UInventoryManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UInventoryManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;

	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

		if (gameInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("InventoryManager: GameInstance found"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("InventoryManager: GameInstance not found"));
		}
	}
}

void UInventoryManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

void UInventoryManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

void UInventoryManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Network Manager found and subscribed to events"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Network Manager is valid"));

			// Subscribe to the network manager's events
			networkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UInventoryManager::ProcessGameServerData);
			networkManager->OnChunkServerDataReceived.AddDynamic(this, &UInventoryManager::ProcessGameServerData);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("InventoryManager: Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Network manager not found"));
	}
}

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
	UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Received event type: %s"), *MessageData.eventType);

	// Handle inventory data response
	if (MessageData.eventType == "getPlayerInventory" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Received inventory data"));
		ProcessInventoryData(ReceivedData);
	}
	// Handle inventory update events
	else if (MessageData.eventType == "INVENTORY_UPDATE" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Received inventory update"));
		ProcessInventoryUpdate(ReceivedData);
	}
	// Handle use item response
	else if (MessageData.eventType == "useItem" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Item used successfully"));
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
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Item dropped successfully"));
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
									UE_LOG(LogTemp, Log, TEXT("InventoryManager: Updated killCount=%d for inventoryItemId=%d"), NewKillCount, InventoryItemId);
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
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Failed to deserialize inventory data"));
		OnInventoryLoadComplete.Broadcast(false);
		return;
	}

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: No body found in inventory data"));
		OnInventoryLoadComplete.Broadcast(false);
		return;
	}


	// Use the shared parser that handles nested "item" sub-object per protocol
	FCharacterInventoryStruct Parsed = JSONParser::DeserializeCharacterInventory(Body);

	// Reject packets belonging to a different character
	if (OwnerCharacterId > 0 && Parsed.characterId > 0 && Parsed.characterId != OwnerCharacterId)
	{
		UE_LOG(LogTemp, Verbose, TEXT("InventoryManager: Ignoring inventory data for CharID=%d (owner=%d)"),
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
	UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Loaded %d items, gold=%d for character %d"),
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
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Failed to deserialize inventory update"));
		return;
	}

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: No body found in inventory update"));
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
			UE_LOG(LogTemp, Verbose, TEXT("InventoryManager: Ignoring INVENTORY_UPDATE for CharID=%d (owner=%d)"), CharacterId, OwnerCharacterId);
			return;
		}

		// Parse simplified items array (only itemId and quantity)
		const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
		if (DataObject->TryGetArrayField(TEXT("items"), ItemsArray))
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *ItemsArray)
			{
				TSharedPtr<FJsonObject> ItemObject = ItemValue->AsObject();
				if (ItemObject.IsValid())
				{
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
						
						UE_LOG(LogTemp, Warning, TEXT("InventoryManager: New item detected (ID: %d), may need full item data"), ItemId);
					}
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

void UInventoryManager::UpdateLocalInventory(const FCharacterInventoryStruct& NewInventory)
{
	FCharacterInventoryStruct OldInventory = CurrentInventory;
	CurrentInventory = NewInventory;

	// Compare old and new inventory to detect changes
	for (const FInventoryItemStruct& NewItem : NewInventory.items)
	{
		FInventoryItemStruct* OldItem = OldInventory.items.FindByPredicate(
			[NewItem](const FInventoryItemStruct& Item) { return Item.itemId == NewItem.itemId; });

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
			[OldItem](const FInventoryItemStruct& Item) { return Item.itemId == OldItem.itemId; });

		if (!bFoundInNew)
		{
			// Item was completely removed
			OnItemRemoved.Broadcast(OldItem, OldItem.quantity);
		}
	}

	OnInventoryUpdated.Broadcast(CurrentInventory);
	UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Inventory updated for character %d"), CurrentInventory.characterId);
}

void UInventoryManager::RequestInventoryData(int32 CharacterId)
{
	if (!networkManager)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Network manager not found"));
		OnInventoryLoadComplete.Broadcast(false);
		return;
	}

	if (!gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Game instance not found"));
		OnInventoryLoadComplete.Broadcast(false);
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
	BodyData.Add(TEXT("id"), CharacterIDValue);
	
	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("getPlayerInventory"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Sent inventory request for character: %d"), CharacterId);
}

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

void UInventoryManager::UseItem(int32 ItemId, int32 Quantity)
{
	if (!HasItem(ItemId))
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Attempted to use item %d which is not in inventory"), ItemId);
		return;
	}

	if (GetItemQuantity(ItemId) < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Not enough quantity of item %d to use"), ItemId);
		return;
	}

	SendUseItemRequest(ItemId, Quantity);
}

void UInventoryManager::DropItem(int32 ItemId, int32 Quantity)
{
	if (!HasItem(ItemId))
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Attempted to drop item %d which is not in inventory"), ItemId);
		return;
	}

	if (GetItemQuantity(ItemId) < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Not enough quantity of item %d to drop"), ItemId);
		return;
	}

	SendDropItemRequest(ItemId, Quantity);
}

void UInventoryManager::SendUseItemRequest(int32 ItemId, int32 Quantity)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Cannot send use item request - missing dependencies"));
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
	
	// Add the item data to the body
	TSharedPtr<FJsonValueNumber> ItemIDValue = MakeShareable(new FJsonValueNumber(ItemId));
	BodyData.Add(TEXT("itemId"), ItemIDValue);
	
	TSharedPtr<FJsonValueNumber> QuantityValue = MakeShareable(new FJsonValueNumber(Quantity));
	BodyData.Add(TEXT("quantity"), QuantityValue);
	
	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("useItem"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Sent use item request for item %d (quantity: %d)"), ItemId, Quantity);
}

void UInventoryManager::SendDropItemRequest(int32 ItemId, int32 Quantity)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Cannot send drop item request - missing dependencies"));
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
	
	// Add the item data to the body
	TSharedPtr<FJsonValueNumber> ItemIDValue = MakeShareable(new FJsonValueNumber(ItemId));
	BodyData.Add(TEXT("itemId"), ItemIDValue);
	
	TSharedPtr<FJsonValueNumber> QuantityValue = MakeShareable(new FJsonValueNumber(Quantity));
	BodyData.Add(TEXT("quantity"), QuantityValue);

	// Include current player position so the server can use an accurate drop location
	// and broadcast the correct ground-level Z to other clients.
	// Without this the server falls back to the last moveCharacter snapshot which may
	// be stale and/or at capsule-center height, causing the item to spawn in the air
	// on observer clients.
	if (ABasicPlayer* CurrentPlayer = gameInstance->Player)
	{
		FClientDataStruct PlayerData = CurrentPlayer->GetPlayerData();
		const FPositionDataStruct& Pos = PlayerData.characterData.characterPosition;
		BodyData.Add(TEXT("posX"),  MakeShareable(new FJsonValueNumber(Pos.positionX)));
		BodyData.Add(TEXT("posY"),  MakeShareable(new FJsonValueNumber(Pos.positionY)));
		BodyData.Add(TEXT("posZ"),  MakeShareable(new FJsonValueNumber(Pos.positionZ)));
		BodyData.Add(TEXT("rotZ"),  MakeShareable(new FJsonValueNumber(Pos.rotationZ)));
		UE_LOG(LogTemp, Log, TEXT("InventoryManager: drop request pos X=%.2f Y=%.2f Z=%.2f"),
			Pos.positionX, Pos.positionY, Pos.positionZ);
	}

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("dropItem"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Sent drop item request for item %d (quantity: %d)"), ItemId, Quantity);
}

void UInventoryManager::PickupNearbyItem()
{
	if (!worldContext)
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: World context not found"));
		return;
	}

	// Prevent pickup while harvesting
	if (gameInstance && gameInstance->GetHarvestManager() && gameInstance->GetHarvestManager()->IsHarvesting())
	{
		return;
	}

	// Get the player character
	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Player pawn not found"));
		return;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Find all dropped item actors in the world
	TArray<AActor*> FoundItems;
	UGameplayStatics::GetAllActorsOfClass(worldContext, ADroppedItemActor::StaticClass(), FoundItems);

	ADroppedItemActor* ClosestItem = nullptr;
	float ClosestDistance = FLT_MAX;

	// Find the closest item within pickup range
	for (AActor* Actor : FoundItems)
	{
		ADroppedItemActor* DroppedItem = Cast<ADroppedItemActor>(Actor);
		if (DroppedItem && DroppedItem->CanBePickedUp())
		{
			float Distance = FVector::Dist(PlayerLocation, DroppedItem->GetActorLocation());
			float ItemPickupRadius = DroppedItem->GetPickupRadius();

			if (Distance <= ItemPickupRadius && Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestItem = DroppedItem;
			}
		}
	}

	// Attempt to pickup the closest item
	if (ClosestItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Attempting to pickup item %s at distance %.2f"),
			*ClosestItem->GetItemName(), ClosestDistance);

		// Rotate the player to face the item, then lock movement.
		if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
		{
			FVector ToItem = ClosestItem->GetActorLocation() - PlayerLocation;
			ToItem.Z = 0.f;
			if (!ToItem.IsNearlyZero())
			{
				Player->SetDesiredFaceYaw(ToItem.Rotation().Yaw);
			}
			Player->LockMovementForPickup();
		}

		bool bPickupSuccess = ClosestItem->AttemptPickup();

		if (bPickupSuccess)
		{
			// Broadcast pickup attempt event
			OnItemPickupAttempted.Broadcast(ClosestItem->GetItemBaseData());
		}
		else
		{
			// Request failed locally (e.g. canBePickedUp == false) — unlock immediately
			if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
			{
				Player->UnlockMovementAfterPickup();
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: No items within pickup range"));
		// Optionally broadcast a "no items found" event
		OnNoItemsInRange.Broadcast();
	}
}

void UInventoryManager::PickupSpecificItem(ADroppedItemActor* TargetItem)
{
    if (!IsValid(TargetItem))
    {
        UE_LOG(LogTemp, Warning, TEXT("InventoryManager: PickupSpecificItem - null target"));
        return;
    }

    if (!TargetItem->CanBePickedUp())
    {
        UE_LOG(LogTemp, Warning, TEXT("InventoryManager: PickupSpecificItem - item cannot be picked up"));
        return;
    }

    if (!worldContext) return;

    APlayerController* PC = worldContext->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;

    ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn());
    if (!Player) return;

    // Guard against starting a new pickup while already picking up —
    // prevents PendingPickupItemUIDs from filling with items that are
    // never cleaned up because the second animation fires first.
    if (Player->IsPickingUp()) return;

    // Prevent starting a pickup while harvesting a corpse
    if (gameInstance && gameInstance->GetHarvestManager() && gameInstance->GetHarvestManager()->IsHarvesting())
    {
        UE_LOG(LogTemp, Warning, TEXT("InventoryManager: PickupSpecificItem blocked - player is harvesting"));
        return;
    }

    FVector PlayerLocation = Player->GetActorLocation();

    // Face the item and lock movement
    {
        FVector ToItem = TargetItem->GetActorLocation() - PlayerLocation;
        ToItem.Z = 0.f;
        if (!ToItem.IsNearlyZero())
        {
            Player->SetDesiredFaceYaw(ToItem.Rotation().Yaw);
        }
        Player->LockMovementForPickup();
    }

    bool bSuccess = TargetItem->AttemptPickup();
    if (bSuccess)
    {
        OnItemPickupAttempted.Broadcast(TargetItem->GetItemBaseData());
    }
    else
    {
        Player->UnlockMovementAfterPickup();
    }

    UE_LOG(LogTemp, Warning, TEXT("InventoryManager: PickupSpecificItem %s - %s"),
        *TargetItem->GetItemName(), bSuccess ? TEXT("OK") : TEXT("FAILED"));
}

ADroppedItemActor* UInventoryManager::GetNearestDroppedItem(float MaxDistance) const
{
	if (!worldContext)
	{
		return nullptr;
	}

	// Get the player character
	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return nullptr;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Find all dropped item actors in the world
	TArray<AActor*> FoundItems;
	UGameplayStatics::GetAllActorsOfClass(worldContext, ADroppedItemActor::StaticClass(), FoundItems);

	ADroppedItemActor* ClosestItem = nullptr;
	float ClosestDistance = MaxDistance + 1.f;

	// Find the closest item within range
	for (AActor* Actor : FoundItems)
	{
		ADroppedItemActor* DroppedItem = Cast<ADroppedItemActor>(Actor);
		if (DroppedItem && DroppedItem->CanBePickedUp())
		{
			float Distance = FVector::Dist(PlayerLocation, DroppedItem->GetActorLocation());

			if (Distance <= DroppedItem->GetPickupRadius() && Distance <= ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestItem = DroppedItem;
			}
		}
	}

	return ClosestItem;
}

TArray<ADroppedItemActor*> UInventoryManager::GetAllDroppedItemsInRange(float MaxDistance) const
{
	TArray<ADroppedItemActor*> ItemsInRange;

	if (!worldContext)
	{
		return ItemsInRange;
	}

	// Get the player character
	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return ItemsInRange;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Find all dropped item actors in the world
	TArray<AActor*> FoundItems;
	UGameplayStatics::GetAllActorsOfClass(worldContext, ADroppedItemActor::StaticClass(), FoundItems);

	// Collect all items within range
	for (AActor* Actor : FoundItems)
	{
		ADroppedItemActor* DroppedItem = Cast<ADroppedItemActor>(Actor);
		if (DroppedItem && DroppedItem->CanBePickedUp())
		{
			float Distance = FVector::Dist(PlayerLocation, DroppedItem->GetActorLocation());
			float ItemPickupRadius = FMath::Min(MaxDistance, DroppedItem->GetPickupRadius());

			if (Distance <= ItemPickupRadius)
			{
				ItemsInRange.Add(DroppedItem);
			}
		}
	}

	// Sort by distance (closest first)
	ItemsInRange.Sort([PlayerLocation](const ADroppedItemActor& A, const ADroppedItemActor& B) {
		float DistA = FVector::Dist(PlayerLocation, A.GetActorLocation());
		float DistB = FVector::Dist(PlayerLocation, B.GetActorLocation());
		return DistA < DistB;
		});

	return ItemsInRange;
}

void UInventoryManager::SetInventoryUIWidget(UInventoryWidget* InInventoryUIWidget)
{
	if (InInventoryUIWidget)
	{
		this->InventoryUIWidget = InInventoryUIWidget;
		InInventoryUIWidget->InitializeInventory(this);

		// Bind UI events
		InInventoryUIWidget->OnInventorySlotClicked.AddDynamic(this, &UInventoryManager::HandleSlotClicked);
		// OnInventorySlotRightClicked is handled entirely by ItemActionMenuWidget - do NOT bind HandleSlotRightClicked here.
		
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: UI Widget set and initialized"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("InventoryManager: Cannot set null UI Widget"));
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
	UE_LOG(LogTemp, Verbose, TEXT("InventoryManager: Slot %d clicked - handled by context menu"), SlotIndex);
}

void UInventoryManager::HandleSlotRightClicked(int32 SlotIndex, const FInventoryItemStruct& Item)
{
	// Right-click is handled by the context menu (ItemActionMenuWidget). No action here.
	UE_LOG(LogTemp, Verbose, TEXT("InventoryManager: Slot %d right-clicked - handled by context menu"), SlotIndex);
}

void UInventoryManager::AddItemToLocalInventory(const FInventoryItemStruct& Item)
{
	// Find existing item with same ID
	FInventoryItemStruct* ExistingItem = CurrentInventory.items.FindByPredicate(
		[Item](const FInventoryItemStruct& InventoryItem) { return InventoryItem.itemId == Item.itemId; });

	if (ExistingItem)
	{
		// Item already exists, add to quantity
		int32 OldQuantity = ExistingItem->quantity;
		ExistingItem->quantity += Item.quantity;
		
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Updated item %s quantity from %d to %d"), 
			*Item.name, OldQuantity, ExistingItem->quantity);
		
		OnItemAdded.Broadcast(*ExistingItem, Item.quantity);
	}
	else
	{
		// New item, add to inventory
		CurrentInventory.items.Add(Item);
		
		UE_LOG(LogTemp, Warning, TEXT("InventoryManager: Added new item %s (ID: %d, Quantity: %d)"), 
			*Item.name, Item.itemId, Item.quantity);
		
		OnItemAdded.Broadcast(Item, Item.quantity);
	}

	// Broadcast inventory update
	OnInventoryUpdated.Broadcast(CurrentInventory);
}
