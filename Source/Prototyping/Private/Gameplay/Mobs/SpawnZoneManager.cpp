// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/SpawnZoneManager.h"
#include "MyGameInstance.h"

USpawnZoneManager::USpawnZoneManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void USpawnZoneManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;
	// Get the game instance
	gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

	if (gameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameInstance found"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameInstance not found"));
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
		UE_LOG(LogTemp, Warning, TEXT("Network Manager found and subscribed to GameServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("Network Manager is valid"));

			networkManager->OnGameServerDataReceived.RemoveDynamic(this, &USpawnZoneManager::ProcessGameServerData);
			networkManager->OnGameServerDataReceived.AddDynamic(this, &USpawnZoneManager::ProcessGameServerData);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void USpawnZoneManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

void USpawnZoneManager::ProcessGameServerData(const FString& ReceivedData)
{
	//UE_LOG(LogTemp, Warning, TEXT("SpawnZoneManager received data: %s"), *ReceivedData);

	// Deserialize the received JSON string to get MessageData struct
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);

	//log the event type
	UE_LOG(LogTemp, Warning, TEXT("SpawnZoneManager: Received event type: %s"), *MessageData.eventType);

	if (MessageData.eventType == "spawnMobsInZone" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnZoneManager: Received spawnMobsInZone event"));
		// Deserialize the JSON string into zone data
		FSpawnZoneStruct SpawnZoneData = JSONParser::DeserializeSpawnZoneData(ReceivedData);

		CreateSpawnZone(SpawnZoneData);
	}
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

	FString JsonString = JSONParser::SerializeJson("getSpawnZoneData", HeaderData, BodyData);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the game server
		networkManager->SendDataToGameServer(JsonString);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void USpawnZoneManager::CreateSpawnZone(const FSpawnZoneStruct& SpawnZoneData)
{
	UE_LOG(LogTemp, Warning, TEXT("Creating spawn zone"));

	FString Tag = SpawnZoneData.zoneName + "_" + FString::FromInt(SpawnZoneData.zoneID);

	// if spawn zone does not exist in the world by tag
	if (SpawnZoneExists(worldContext, FName(Tag)))
	{
		UE_LOG(LogTemp, Warning, TEXT("Spawn Zone already exists"));
		return;
	}


	// Get the game instance
	if (gameInstance)
	{
		// get basic MOB class
		TSubclassOf<class AMobSpawnZone> BasicSpawnZoneClass = gameInstance->GetBasicSpawnZoneClass();

		if (BasicSpawnZoneClass == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Basic Spawn Zone BP Class not found"));
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
			UE_LOG(LogTemp, Error, TEXT("Spawn Zone not created"));
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

// Check if a Spawn Zone exists in the world
bool USpawnZoneManager::SpawnZoneExists(UWorld* World, const FName& Tag)
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(World, Tag, FoundActors);

	// If the array is not empty, then actors with the tag exist
	return FoundActors.Num() > 0;
}