// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"

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
	else if (MessageData.eventType == "nearbyItems")
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemManager: Received nearbyItems event"));
		// Body contains { "items": [ DroppedItemStruct... ] } - same shape as itemDrop
		FItemDropResponseStruct NearbyResponse = JSONParser::DeserializeItemDropResponse(Body);
		for (const FDroppedItemStruct& DroppedItem : NearbyResponse.droppedItems)
		{
			if (DroppedItem.uid > 0)
				SpawnDroppedItem(DroppedItem);
		}
	}
	else if (MessageData.eventType == "itemPickup" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemManager: Received pickUpItem event"));

		// Cache item data from server confirmation
		if (Body->HasField(TEXT("item")))
		{
			PendingPickupItem = JSONParser::DeserializeItemData(Body->GetObjectField(TEXT("item")));
		}

		// Cache the dropped item UID so OnPickupPointFired knows which actor to destroy
		PendingPickupItemUID = -1;
		Body->TryGetNumberField(TEXT("droppedItemUID"), PendingPickupItemUID);

		// Bind the pickup-point delegate once (lazy, first successful pickup)
		BindPickupPointDelegate();

		// Try to get the local player and play the pickup montage.
		// Movement was already locked in InventoryManager::PickupNearbyItem.
		bool bMontageStarted = false;
		if (worldContext)
		{
			if (APlayerController* PC = worldContext->GetFirstPlayerController())
			{
				if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
				{
					if (UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(
						Player->GetMesh()->GetAnimInstance()))
					{
						const float MontageDuration = AnimInst->NotifyPickup();
						bMontageStarted = MontageDuration > 0.0f;
						UE_LOG(LogTemp, Warning,
							TEXT("ItemManager: pickup montage started, duration=%.3fs"),
							MontageDuration);
					}
				}
			}
		}

		// Fallback: no player / no montage assigned — fire immediately
		if (!bMontageStarted)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ItemManager: no pickup montage, resolving pickup immediately"));
			OnPickupPointFired();
		}
	}
	else if (MessageData.eventType == "itemPickup" && MessageData.status != "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemManager: itemPickup failed — unlocking player movement"));

		// Server rejected the pickup — cancel any pending pickup-point timer and unlock
		if (worldContext)
		{
			if (APlayerController* PC = worldContext->GetFirstPlayerController())
			{
				if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
				{
					if (UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(
						Player->GetMesh()->GetAnimInstance()))
					{
						AnimInst->CancelPickupTimer();
					}
					Player->UnlockMovementAfterPickup();
				}
			}
		}

		PendingPickupItemUID = -1;
		PendingPickupItem    = FItemBaseStruct();
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

	PreloadNiagaraAssetsAsync();
}

void UItemManager::PreloadNiagaraAssetsAsync()
{
	TArray<FSoftObjectPath> Paths;
	Paths.Reserve(ItemVisualsCache.Num() * 4);

	for (const auto& Pair : ItemVisualsCache)
	{
		const FItemVisualData& V = Pair.Value;
		if (!V.DropNiagaraSystem.IsNull())    Paths.Add(V.DropNiagaraSystem.ToSoftObjectPath());
		if (!V.PickupNiagaraSystem.IsNull())  Paths.Add(V.PickupNiagaraSystem.ToSoftObjectPath());
		if (!V.EquippedSwingVFX.IsNull())     Paths.Add(V.EquippedSwingVFX.ToSoftObjectPath());
		if (!V.EquippedIdleVFX.IsNull())      Paths.Add(V.EquippedIdleVFX.ToSoftObjectPath());
	}

	if (Paths.Num() == 0) return;

	FStreamableManager& SM = UAssetManager::GetStreamableManager();
	NiagaraPreloadHandle = SM.RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateLambda([](){}),
		FStreamableManager::AsyncLoadHighPriority
	);

	UE_LOG(LogTemp, Log, TEXT("ItemManager: Preloading %d Niagara VFX assets"), Paths.Num());
}

FItemVisualData UItemManager::GetItemVisualDataBySlug(const FString& ItemSlug)
{
	if (ItemSlug.IsEmpty()) return FItemVisualData();

	// Check cache first
	if (ItemVisualsCache.Contains(ItemSlug))
	{
		return ItemVisualsCache[ItemSlug];
	}

	if (ItemVisualsDataTable)
	{
		// Row name IS the slug — same convention as FItemLocaleDefinition
		FItemVisualData* VisualData = ItemVisualsDataTable->FindRow<FItemVisualData>(FName(*ItemSlug), TEXT("GetItemVisualDataBySlug"), false);
		if (VisualData)
		{
			ItemVisualsCache.Add(ItemSlug, *VisualData);
			return *VisualData;
		}

		// Fallback: legacy rows where ItemSlug field was used instead of row name
		for (const FName& RowName : ItemVisualsDataTable->GetRowNames())
		{
			FItemVisualData* Row = ItemVisualsDataTable->FindRow<FItemVisualData>(RowName, TEXT("GetItemVisualDataBySlug"), false);
			if (Row && Row->ItemSlug == ItemSlug)
			{
				ItemVisualsCache.Add(ItemSlug, *Row);
				return *Row;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("ItemManager: No visual data found for slug: %s"), *ItemSlug);
	return FItemVisualData();
}

// ---------------------------------------------------------------------------
// BindPickupPointDelegate
//   Lazily binds to the local player's PlayerAnimInstance::OnPickupPoint so
//   OnPickupPointFired() is called at the exact animation frame.
//   Safe to call multiple times — binds only once per session.
// ---------------------------------------------------------------------------
void UItemManager::BindPickupPointDelegate()
{
	if (bPickupDelegateBound || !worldContext) return;

	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC) return;

	ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn());
	if (!Player) return;

	UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(
		Player->GetMesh()->GetAnimInstance());
	if (!AnimInst) return;

	AnimInst->OnPickupPoint.AddUObject(this, &UItemManager::OnPickupPointFired);
	bPickupDelegateBound = true;

	UE_LOG(LogTemp, Log, TEXT("ItemManager: bound OnPickupPoint delegate"));
}

// ---------------------------------------------------------------------------
// OnPickupPointFired
//   Called by AnimNotify_PickupPoint (or the fallback timer inside
//   PlayerAnimInstance::NotifyPickup) at the visual moment of pickup.
//   1. Plays the Niagara pickup VFX and destroys the DroppedItemActor.
//   2. Broadcasts ProcessItemPickup so inventory / UI refresh happens here.
//   3. Unlocks player movement.
//   4. Triggers inventory refresh from server.
// ---------------------------------------------------------------------------
void UItemManager::OnPickupPointFired()
{
	UE_LOG(LogTemp, Warning, TEXT("ItemManager: OnPickupPointFired uid=%d"), PendingPickupItemUID);

	// Destroy the world item and play pickup VFX
	if (PendingPickupItemUID > 0 && DroppedItemsMap.Contains(PendingPickupItemUID))
	{
		ADroppedItemActor* DroppedActor = DroppedItemsMap[PendingPickupItemUID];
		if (IsValid(DroppedActor))
		{
			DroppedActor->PlayPickupEffect();
		}
		DroppedItemsMap.Remove(PendingPickupItemUID);
	}

	// Broadcast item to inventory/UI systems
	if (!PendingPickupItem.name.IsEmpty())
	{
		ProcessItemPickup(PendingPickupItem);
	}

	// Unlock player movement
	if (worldContext)
	{
		if (APlayerController* PC = worldContext->GetFirstPlayerController())
		{
			if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
			{
				Player->UnlockMovementAfterPickup();
			}
		}
	}

	// Refresh inventory from server
	if (gameInstance && gameInstance->GetInventoryManager())
	{
		gameInstance->GetInventoryManager()->RequestInventoryData(gameInstance->GetCurrentCharacterID());
	}

	// Reset pending state
	PendingPickupItemUID = -1;
	PendingPickupItem    = FItemBaseStruct();
}