#include "Gameplay/WorldObjects/WIONetworkHandler.h"
#include "Prototyping.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
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
		UE_LOG(LogGameplay, Error, TEXT("WIONetworkHandler: Initialize called with null parameters"));
		return;
	}
	WorldObjectManager = InManager;
	NetworkManager     = InNetworkManager;
}

void UWIONetworkHandler::SubscribeToNetworkEvents()
{
	if (!NetworkManager || !IsValid(NetworkManager))
	{
		UE_LOG(LogGameplay, Error, TEXT("WIONetworkHandler: Cannot subscribe — NetworkManager is null"));
		return;
	}
	if (bIsSubscribed)
	{
		return;
	}
	FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
	for (const FString& EventType : HandledEvents())
	{
		Dispatcher.AddHandler(EventType, this, &UWIONetworkHandler::HandleChunkPacket);
	}
	bIsSubscribed = true;
}

void UWIONetworkHandler::UnsubscribeFromNetworkEvents()
{
	if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed)
	{
		return;
	}
	FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
	for (const FString& EventType : HandledEvents())
	{
		Dispatcher.RemoveHandler(EventType, this);
	}
	bIsSubscribed = false;
}

// EventTypes owned by this handler (single list drives dispatcher registration).
const TArray<FString>& UWIONetworkHandler::HandledEvents()
{
	static const TArray<FString> Events = {
		TEXT("spawnWorldObjects"),
		TEXT("worldObjectInteractResult"),
		TEXT("worldObjectStateUpdate"),
		TEXT("worldObjectChannelCancelled")
	};
	return Events;
}

void UWIONetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
	if (!Packet.bValid || !WorldObjectManager || !Packet.Body.IsValid())
	{
		return;
	}

	// Single-parse path (S05): EventType/Body come from PollLink, no re-parse.
	const FString& EventType = Packet.EventType;
	const TSharedPtr<FJsonObject>& Body = Packet.Body;

	if (EventType == TEXT("spawnWorldObjects"))
	{
		TArray<FWorldObjectData> Objects = JSONParser::DeserializeWorldObjectsList(Body);
		WorldObjectManager->HandleSpawnWorldObjects(Objects);
	}
	else if (EventType == TEXT("worldObjectInteractResult"))
	{
		FWIOInteractResult Result = JSONParser::DeserializeWIOInteractResult(Body);
		WorldObjectManager->HandleInteractResult(Result);
	}
	else if (EventType == TEXT("worldObjectStateUpdate"))
	{
		FWIOStateUpdate Update = JSONParser::DeserializeWIOStateUpdate(Body);
		WorldObjectManager->HandleStateUpdate(Update);
	}
	else if (EventType == TEXT("worldObjectChannelCancelled"))
	{
		FWIOChannelCancelled Cancelled = JSONParser::DeserializeWIOChannelCancelled(Body);
		WorldObjectManager->HandleChannelCancelled(Cancelled.ObjectId);
	}
}
