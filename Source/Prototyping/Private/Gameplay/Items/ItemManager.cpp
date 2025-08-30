// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
UItemManager::UItemManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.
	PrimaryComponentTick.bCanEverTick = true;

	// Initialize references
	networkManager = nullptr;
	gameInstance = nullptr;
	worldContext = nullptr;
}

void UItemManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;

	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

		if (gameInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("GameInstance found in ItemManager"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GameInstance not found in ItemManager"));
		}
	}
}

void UItemManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

void UItemManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

void UItemManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemManager: Network Manager found and subscribed to ChunkServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("ItemManager: Network Manager is valid"));

			// Subscribe to the network manager's events
			networkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UItemManager::ProcessGameServerData);
			networkManager->OnChunkServerDataReceived.AddDynamic(this, &UItemManager::ProcessGameServerData);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ItemManager: Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ItemManager: Network manager not found"));
	}
}

void UItemManager::ProcessGameServerData(const FString& ReceivedData)
{
	// Process time sync data first
	//if (gameInstance && gameInstance->GetTimeSyncService())
	//{
	//	JSONParser::ProcessTimeSyncFromHeader(ReceivedData, gameInstance->GetTimeSyncService());
	//}

	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	UE_LOG(LogTemp, Warning, TEXT("ItemManager: Received event type: %s"), *MessageData.eventType);

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));

	if (MessageData.eventType == "itemDrop" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemManager: Received itemDrop event"));
		FItemDropResponseStruct ItemDropResponse = JSONParser::DeserializeItemDropResponse(Body);
		HandleItemDrop(ItemDropResponse);
	}
	else if (MessageData.eventType == "itemPickup" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemManager: Received pickUpItem event"));
		
		// Process item pickup if available in the response
		if (Body->HasField(TEXT("item")))
		{
			FItemBaseStruct PickedUpItem = JSONParser::DeserializeItemData(Body->GetObjectField(TEXT("item")));
			ProcessItemPickup(PickedUpItem);
		}
		
		// Handle removing item from the world
		int32 ItemUID = -1;
		if (Body->TryGetNumberField(TEXT("droppedItemUID"), ItemUID) && ItemUID > 0)
		{
			// Find and remove the item actor from the world
			if (DroppedItemsMap.Contains(ItemUID))
			{
				if (DroppedItemsMap[ItemUID] != nullptr)
				{
					DroppedItemsMap[ItemUID]->Destroy();
				}
				DroppedItemsMap.Remove(ItemUID);
			}
		}

		// Notify inventory manager to update inventory
		if (gameInstance && gameInstance->GetInventoryManager())
		{
			gameInstance->GetInventoryManager()->RequestInventoryData(gameInstance->GetCurrentCharacterID());
		}
	}
}

void UItemManager::HandleItemDrop(const FItemDropResponseStruct& ItemDropResponse)
{
	// Spawn actors for all dropped items
	for (const FDroppedItemStruct& DroppedItem : ItemDropResponse.droppedItems)
	{
		SpawnDroppedItem(DroppedItem);
	}

	// Broadcast the event
	if (ItemDropResponse.droppedItems.Num() > 0)
	{
		OnItemsDropped.Broadcast(ItemDropResponse.droppedItems);
	}
}

ADroppedItemActor* UItemManager::SpawnDroppedItem(const FDroppedItemStruct& DroppedItem)
{
	// If this item already exists in the world, don't spawn it again
	if (ItemExists(worldContext, DroppedItem.uid))
	{
		UE_LOG(LogTemp, Warning, TEXT("Item with UID %d already exists in the world"), DroppedItem.uid);
		return nullptr;
	}

	// Make sure we have a game instance and world context
	if (!gameInstance || !worldContext)
	{
		UE_LOG(LogTemp, Error, TEXT("GameInstance or WorldContext not found in ItemManager"));
		return nullptr;
	}

	// Get the DroppedItemActor class from game instance
	TSubclassOf<class ADroppedItemActor> DroppedItemClass = gameInstance->GetDroppedItemActorClass();

	if (DroppedItemClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("DroppedItemActor class not found in GameInstance"));
		return nullptr;
	}

	// Create the spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Create the transform from the position data
	FVector Location(
		DroppedItem.position.positionX,
		DroppedItem.position.positionY,
		DroppedItem.position.positionZ
	);
	
	FRotator Rotation(0, DroppedItem.position.rotationZ, 0);
	FTransform Transform(Rotation, Location);

	// Spawn the actor
	ADroppedItemActor* SpawnedItem = worldContext->SpawnActorDeferred<ADroppedItemActor>(
		DroppedItemClass, 
		Transform, 
		nullptr, 
		nullptr, 
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (SpawnedItem)
	{
		// Set the item data before finalizing the spawn
		SpawnedItem->SetItemData(DroppedItem);
		UGameplayStatics::FinishSpawningActor(SpawnedItem, Transform);
		
		// Add to the map
		DroppedItemsMap.Add(DroppedItem.uid, SpawnedItem);
		
		UE_LOG(LogTemp, Warning, TEXT("Spawned item %s (ID:%d) at location X:%f Y:%f Z:%f"), 
			*DroppedItem.item.name, DroppedItem.uid, Location.X, Location.Y, Location.Z);
		
		return SpawnedItem;
	}
	
	UE_LOG(LogTemp, Error, TEXT("Failed to spawn item %s (ID:%d)"), *DroppedItem.item.name, DroppedItem.uid);
	return nullptr;
}

bool UItemManager::ItemExists(UWorld* World, int32 ItemUID)
{
	// Check if the item exists in our map
	if (DroppedItemsMap.Contains(ItemUID) && DroppedItemsMap[ItemUID] != nullptr)
	{
		return true;
	}
	
	return false;
}

void UItemManager::SendPickUpItemRequest(int32 ItemUID)
{
	if (!networkManager)
	{
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
		return;
	}

	if (!gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("Game instance not found"));
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
	
	// Add the item UID to the body data
	TSharedPtr<FJsonValueNumber> ItemUIDValue = MakeShareable(new FJsonValueNumber(ItemUID));
	BodyData.Add(TEXT("itemUID"), ItemUIDValue);

	// add character ID to the body data
	int32 CharacterId = gameInstance->GetCurrentCharacterID();
	TSharedPtr<FJsonValueNumber> CharacterIDValue = MakeShareable(new FJsonValueNumber(CharacterId));
	BodyData.Add(TEXT("characterId"), CharacterIDValue);

	// Get ABasicPlayer and his location
	ABasicPlayer* CurrentPlayer = gameInstance->Player;
	if (CurrentPlayer)
	{
		// Get player data from the current player
		FClientDataStruct PlayerData = CurrentPlayer->GetPlayerData();
		FPositionDataStruct PlayerPosition = PlayerData.characterData.characterPosition;

		// Add player position to the body data
		TSharedPtr<FJsonValueNumber> PosXValue = MakeShareable(new FJsonValueNumber(PlayerPosition.positionX));
		BodyData.Add(TEXT("posX"), PosXValue);

		TSharedPtr<FJsonValueNumber> PosYValue = MakeShareable(new FJsonValueNumber(PlayerPosition.positionY));
		BodyData.Add(TEXT("posY"), PosYValue);

		TSharedPtr<FJsonValueNumber> PosZValue = MakeShareable(new FJsonValueNumber(PlayerPosition.positionZ));
		BodyData.Add(TEXT("posZ"), PosZValue);

		TSharedPtr<FJsonValueNumber> RotZValue = MakeShareable(new FJsonValueNumber(PlayerPosition.rotationZ));
		BodyData.Add(TEXT("rotZ"), RotZValue);

		UE_LOG(LogTemp, Log, TEXT("Added player position to pickup request: X=%.2f, Y=%.2f, Z=%.2f, RotZ=%.2f"),
			PlayerPosition.positionX, PlayerPosition.positionY, PlayerPosition.positionZ, PlayerPosition.rotationZ);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Current player not found, sending pickup request without position data"));
	}

	
	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("itemPickup"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("Sent itemPickup request for item UID: %d"), ItemUID);
}

void UItemManager::ProcessItemPickup(const FItemBaseStruct& Item)
{
	// Broadcast the item pickup event
	OnItemPickedUp.Broadcast(Item);
	
	UE_LOG(LogTemp, Warning, TEXT("Picked up item: %s"), *Item.name);
}

// Called when the game starts
void UItemManager::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void UItemManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UItemManager::LoadItemVisualsDataTable(UDataTable* InItemVisualsTable)
{
	ItemVisualsDataTable = InItemVisualsTable;

	// Clear existing cache
	ItemVisualsCache.Empty();

	// If we have a valid data table, populate the cache
	if (ItemVisualsDataTable)
	{
		TArray<FName> RowNames = ItemVisualsDataTable->GetRowNames();

		for (const FName& RowName : RowNames)
		{
			FItemVisualData* VisualData = ItemVisualsDataTable->FindRow<FItemVisualData>(RowName, TEXT("Loading item visuals"));

			if (VisualData && !VisualData->ItemSlug.IsEmpty())
			{
				ItemVisualsCache.Add(VisualData->ItemSlug, *VisualData);
				UE_LOG(LogTemp, Log, TEXT("Cached visual data for item: %s"), *VisualData->ItemSlug);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Loaded %d item visual entries from data table"), ItemVisualsCache.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No item visuals data table provided"));
	}
}

FItemVisualData UItemManager::GetItemVisualDataBySlug(const FString& ItemSlug)
{
	// Check if we have the visual data cached
	if (ItemVisualsCache.Contains(ItemSlug))
	{
		return ItemVisualsCache[ItemSlug];
	}

	// If not in cache but we have a data table, try to find it
	if (ItemVisualsDataTable)
	{
		// Try to find by slug by iterating through rows
		TArray<FName> RowNames = ItemVisualsDataTable->GetRowNames();
		for (const FName& RowName : RowNames)
		{
			FItemVisualData* VisualData = ItemVisualsDataTable->FindRow<FItemVisualData>(RowName, TEXT("Getting item visual data"));

			if (VisualData && VisualData->ItemSlug == ItemSlug)
			{
				// Cache for future use
				ItemVisualsCache.Add(ItemSlug, *VisualData);
				return *VisualData;
			}
		}
	}

	// Return empty data if nothing found
	UE_LOG(LogTemp, Warning, TEXT("No visual data found for item slug: %s"), *ItemSlug);
	return FItemVisualData();
}