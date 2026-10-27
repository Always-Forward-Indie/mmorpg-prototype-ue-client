// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/SpawnZoneManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Services/TimeSyncService.h"
#include "CrashDiagnostics.h"

USpawnZoneManager::USpawnZoneManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void USpawnZoneManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;
	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());
	}

	if (gameInstance)
	{
		UE_LOG(LogGameplay, Warning, TEXT("GameInstance found"));
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("GameInstance not found"));
	}
}

void USpawnZoneManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

// subscribe to the network manager's event
void USpawnZoneManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogGameplay, Warning, TEXT("Network Manager found and subscribed to ChunkServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogGameplay, Warning, TEXT("Network Manager is valid"));

			// Single-parse path (S05): dispatcher fallback delivers the parsed packet.
			networkManager->DispatcherFor(EServerType::ChunkServer).RemoveFallbackHandler(this);
			networkManager->DispatcherFor(EServerType::ChunkServer).AddFallbackHandler(this, &USpawnZoneManager::HandleChunkPacket);
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("Network manager not found"));
	}
}

void USpawnZoneManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

void USpawnZoneManager::ClearWorldState()
{
	SpawnZones.Empty();
}

void USpawnZoneManager::HandleChunkPacket(const FParsedPacket& Packet)
{
    CRASH_GUARD("SpawnZoneManager::HandleChunkPacket");
    if (!worldContext) return;

	// Time sync is handled by the dispatcher fallback.

	// Deserialize the received JSON string to get MessageData struct
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(Packet);

	//log the event type
	UE_LOG(LogGameplay, Warning, TEXT("SpawnZoneManager: Received event type: %s"), *MessageData.eventType);

	// NOTE: spawnMobsInZone no longer contains spawn zone geometry data (removed server-side v0.2.16).
	// Spawn zone creation from server packets is no longer supported.
	// Mob spawning from this event is handled by MOBManager.
}

void USpawnZoneManager::SendGetSpawnZonesData(const FClientDataStruct& ClientData)
{
	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add the client ID and hash to the header data
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	// Add the MOB ID to the body data
	TSharedPtr<FJsonValueNumber> zoneID = MakeShareable(new FJsonValueNumber(1));
	BodyData.Add("spawnZoneId", zoneID);

	// Create JSON string with correct server type  
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("getSpawnZoneData", HeaderData, BodyData, nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the game server
		networkManager->SendDataToGameServer(JsonString);
	}
	else {
		UE_LOG(LogGameplay, Error, TEXT("Network manager not found"));
	}
}

void USpawnZoneManager::CreateSpawnZone(const FSpawnZoneStruct& SpawnZoneData)
{
	UE_LOG(LogGameplay, Warning, TEXT("Creating spawn zone"));

	if (!worldContext || !worldContext->IsValidLowLevel())
	{
		UE_LOG(LogGameplay, Error, TEXT("SpawnZoneManager: Cannot create spawn zone - no valid world context"));
		return;
	}

	FString Tag = SpawnZoneData.zoneName + "_" + FString::FromInt(SpawnZoneData.zoneID);

	// if spawn zone does not exist in the world by tag
	if (SpawnZoneExists(worldContext, FName(Tag)))
	{
		UE_LOG(LogGameplay, Warning, TEXT("Spawn Zone already exists"));
		return;
	}


	// Get the game instance
	if (gameInstance)
	{
		// get basic MOB class
		TSubclassOf<class AMobSpawnZone> BasicSpawnZoneClass = gameInstance->GetBasicSpawnZoneClass();

		if (BasicSpawnZoneClass == nullptr)
		{
			UE_LOG(LogGameplay, Error, TEXT("Basic Spawn Zone BP Class not found"));
			return;
		}

		// Create the SpawnZone in the world
		AMobSpawnZone* BasicSpawnZone = worldContext->SpawnActor<AMobSpawnZone>(
			BasicSpawnZoneClass,
			FVector(SpawnZoneData.spawnStartPos),
			FRotator(0, 0, 0)
		);

		if (BasicSpawnZone == nullptr)
		{
			UE_LOG(LogGameplay, Error, TEXT("Spawn Zone not created"));
			return;
		}

		// Set the spawn zone data
		BasicSpawnZone->SetSpawnZoneData(SpawnZoneData);
		// Set the spawn zone status
		BasicSpawnZone->SetSpawnZoneStatus(true);
		// Set the spawn zone tag to the zone ID
		BasicSpawnZone->SetSpawnZoneTag(FName(Tag));

		// Add the spawn zone to the list of spawn zones
		SpawnZones.Add(BasicSpawnZone);
	}
}

// Check if a Spawn Zone exists in the registry (tag match, no world scan)
bool USpawnZoneManager::SpawnZoneExists(UWorld* World, const FName& Tag)
{
	if (!World || !World->IsValidLowLevel())
	{
		UE_LOG(LogGameplay, Error, TEXT("SpawnZoneManager::SpawnZoneExists: Invalid world context"));
		return false;
	}

	for (AMobSpawnZone* Zone : SpawnZones)
	{
		if (IsValid(Zone) && Zone->ActorHasTag(Tag))
		{
			return true;
		}
	}
	return false;
}