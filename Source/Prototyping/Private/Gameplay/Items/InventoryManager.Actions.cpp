#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Services/TimeSyncService.h"
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

void UInventoryManager::UseItem(int32 ItemId, int32 Quantity)
{
	if (!HasItem(ItemId))
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Attempted to use item %d which is not in inventory"), ItemId);
		return;
	}

	if (GetItemQuantity(ItemId) < Quantity)
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Not enough quantity of item %d to use"), ItemId);
		return;
	}

	SendUseItemRequest(ItemId, Quantity);
}

void UInventoryManager::DropItem(int32 ItemId, int32 Quantity)
{
	if (!HasItem(ItemId))
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Attempted to drop item %d which is not in inventory"), ItemId);
		return;
	}

	if (GetItemQuantity(ItemId) < Quantity)
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Not enough quantity of item %d to drop"), ItemId);
		return;
	}

	SendDropItemRequest(ItemId, Quantity);
}

void UInventoryManager::SendUseItemRequest(int32 ItemId, int32 Quantity)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogNetPacket, Error, TEXT("InventoryManager: Cannot send use item request - missing dependencies"));
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
	// Server always consumes exactly 1 per use — no quantity field.
	
	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("useItem"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Sent use item request for item %d (quantity: %d)"), ItemId, Quantity);
}

void UInventoryManager::SendDropItemRequest(int32 ItemId, int32 Quantity)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogNetPacket, Error, TEXT("InventoryManager: Cannot send drop item request - missing dependencies"));
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
		UE_LOG(LogGameplay, Verbose, TEXT("InventoryManager: drop request pos X=%.2f Y=%.2f Z=%.2f"),
			Pos.positionX, Pos.positionY, Pos.positionZ);
	}

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("dropItem"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Sent drop item request for item %d (quantity: %d)"), ItemId, Quantity);
}

