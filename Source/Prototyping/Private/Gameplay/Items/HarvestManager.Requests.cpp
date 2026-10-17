#include "Gameplay/Items/HarvestManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "TimerManager.h"
#include "CrashDiagnostics.h"

void UHarvestManager::SendHarvestStartRequest(int32 CorpseUID)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot send harvest start request - missing dependencies"));
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

	TSharedPtr<FJsonValueString> MessageValue = MakeShareable(new FJsonValueString(TEXT("start harvest")));
	HeaderData.Add(TEXT("message"), MessageValue);
	
	// Add the corpse UID to the body data
	TSharedPtr<FJsonValueNumber> CorpseUIDValue = MakeShareable(new FJsonValueNumber(CorpseUID));
	BodyData.Add(TEXT("corpseUID"), CorpseUIDValue);
	
	// Create the JSON string with automatic clientSendMs and correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("harvestStart"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Sent harvest start request for corpse: %d"), CorpseUID);
}

void UHarvestManager::SendLootPickupRequest(int32 CorpseUID, const TArray<FCorpseLootPickupRequestItem>& RequestedItems)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot send loot pickup request - missing dependencies"));
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

	TSharedPtr<FJsonValueString> MessageValue = MakeShareable(new FJsonValueString(TEXT("pickup corpse loot")));
	HeaderData.Add(TEXT("message"), MessageValue);
	
	// Add player ID and corpse UID to the body data
	// Server validates playerId == characterId (from session), so send characterId here.
	TSharedPtr<FJsonValueNumber> PlayerIDValue = MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentCharacterID()));
	BodyData.Add(TEXT("playerId"), PlayerIDValue);

	TSharedPtr<FJsonValueNumber> CorpseUIDValue = MakeShareable(new FJsonValueNumber(CorpseUID));
	BodyData.Add(TEXT("corpseUID"), CorpseUIDValue);

	// Create requested items array
	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	for (const FCorpseLootPickupRequestItem& Item : RequestedItems)
	{
		TSharedPtr<FJsonObject> ItemObj = MakeShareable(new FJsonObject);
		ItemObj->SetNumberField(TEXT("itemId"), Item.itemId);
		ItemObj->SetNumberField(TEXT("quantity"), Item.quantity);
		ItemsArray.Add(MakeShareable(new FJsonValueObject(ItemObj)));
	}

	TSharedPtr<FJsonValueArray> RequestedItemsArray = MakeShareable(new FJsonValueArray(ItemsArray));
	BodyData.Add(TEXT("requestedItems"), RequestedItemsArray);
	
	// Create the JSON string with automatic clientSendMs and correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("corpseLootPickup"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Sent loot pickup request for corpse: %d with %d items"), 
		CorpseUID, RequestedItems.Num());
}

void UHarvestManager::SendLootInspectRequest(int32 CorpseUID)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestManager: Cannot send loot inspect request - missing dependencies"));
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

	TSharedPtr<FJsonValueString> MessageValue = MakeShareable(new FJsonValueString(TEXT("inspect corpse loot")));
	HeaderData.Add(TEXT("message"), MessageValue);
	
	// Add player ID and corpse UID to the body data
	// Server validates playerId == characterId (from session), so send characterId here.
	TSharedPtr<FJsonValueNumber> PlayerIDValue = MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentCharacterID()));
	BodyData.Add(TEXT("playerId"), PlayerIDValue);

	TSharedPtr<FJsonValueNumber> CorpseUIDValue = MakeShareable(new FJsonValueNumber(CorpseUID));
	BodyData.Add(TEXT("corpseUID"), CorpseUIDValue);
	
	// Create the JSON string with automatic clientSendMs and correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("corpseLootInspect"), HeaderData, BodyData, gameInstance->GetTimeSyncService(), EServerType::ChunkServer);
	
	// Send the request to the server
	networkManager->SendDataToChunkServer(JsonString);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestManager: Sent loot inspect request for corpse: %d"), CorpseUID);
}

