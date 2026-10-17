// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationToastWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"
#include "CrashDiagnostics.h"

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

	// If the item was dropped by a mob that is still alive, defer the spawn.
	// The drop will appear on the ground only after the mob dies (FlushDropsForMob).
	if (!DroppedItem.droppedByMobUID.IsEmpty())
	{
		const int32 MobUID = FCString::Atoi(*DroppedItem.droppedByMobUID);
		if (MobUID > 0)
		{
			TArray<AActor*> MobActors;
			UGameplayStatics::GetAllActorsWithTag(worldContext, FName(*DroppedItem.droppedByMobUID), MobActors);
			if (MobActors.Num() > 0)
			{
				if (ABasicMOB* Mob = Cast<ABasicMOB>(MobActors[0]))
				{
					if (!Mob->GetMOBIsDead())
					{
						UE_LOG(LogTemp, Log, TEXT("ItemManager: deferring item uid=%d \u2014 mob %d still alive"),
							DroppedItem.uid, MobUID);
						PendingMobDrops.FindOrAdd(MobUID).Add(DroppedItem);
						return nullptr;
					}
				}
			}
		}
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

