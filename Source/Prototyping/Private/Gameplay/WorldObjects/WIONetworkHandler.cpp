#include "Gameplay/WorldObjects/WIONetworkHandler.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

UWIONetworkHandler::UWIONetworkHandler()
{
}

void UWIONetworkHandler::Initialize(UWorldObjectManager* InManager, UNetworkManager* InNetworkManager)
{
	if (!InManager || !InNetworkManager)
	{
		UE_LOG(LogTemp, Error, TEXT("WIONetworkHandler: Initialize called with null parameters"));
		return;
	}
	WorldObjectManager = InManager;
	NetworkManager     = InNetworkManager;
}

void UWIONetworkHandler::SubscribeToNetworkEvents()
{
	if (!NetworkManager || !IsValid(NetworkManager))
	{
		UE_LOG(LogTemp, Error, TEXT("WIONetworkHandler: Cannot subscribe — NetworkManager is null"));
		return;
	}
	if (bIsSubscribed)
	{
		return;
	}
	NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UWIONetworkHandler::HandleChunkServerData);
	bIsSubscribed = true;
}

void UWIONetworkHandler::UnsubscribeFromNetworkEvents()
{
	if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed)
	{
		return;
	}
	NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UWIONetworkHandler::HandleChunkServerData);
	bIsSubscribed = false;
}

void UWIONetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
	if (ReceivedData.IsEmpty() || !WorldObjectManager)
	{
		return;
	}

	// Quick event type extraction before full parse
	FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
	const FString& EventType = Msg.eventType;

	if (EventType != TEXT("spawnWorldObjects") &&
		EventType != TEXT("worldObjectInteractResult") &&
		EventType != TEXT("worldObjectStateUpdate") &&
		EventType != TEXT("worldObjectChannelCancelled"))
	{
		return;
	}

	// Full JSON parse
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("WIONetworkHandler: Failed to parse JSON for event %s"), *EventType);
		return;
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("WIONetworkHandler: Missing body in event %s"), *EventType);
		return;
	}

	if (EventType == TEXT("spawnWorldObjects"))
	{
		TArray<FWorldObjectData> Objects = JSONParser::DeserializeWorldObjectsList(*BodyPtr);
		WorldObjectManager->HandleSpawnWorldObjects(Objects);
	}
	else if (EventType == TEXT("worldObjectInteractResult"))
	{
		FWIOInteractResult Result = JSONParser::DeserializeWIOInteractResult(*BodyPtr);
		WorldObjectManager->HandleInteractResult(Result);
	}
	else if (EventType == TEXT("worldObjectStateUpdate"))
	{
		FWIOStateUpdate Update = JSONParser::DeserializeWIOStateUpdate(*BodyPtr);
		WorldObjectManager->HandleStateUpdate(Update);
	}
	else if (EventType == TEXT("worldObjectChannelCancelled"))
	{
		FWIOChannelCancelled Cancelled = JSONParser::DeserializeWIOChannelCancelled(*BodyPtr);
		WorldObjectManager->HandleChannelCancelled(Cancelled.ObjectId);
	}
}
