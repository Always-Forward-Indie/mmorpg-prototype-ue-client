// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBManager.h"
#include "MyGameInstance.h"

//constructor
UMOBManager::UMOBManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UMOBManager::Initialize(UNetworkManager* NetworkManager)
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

void UMOBManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

// subscribe to the network manager's event
void UMOBManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Network Manager found and subscribed to GameServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("Network Manager is valid"));

			networkManager->OnGameServerDataReceived.RemoveDynamic(this, &UMOBManager::ProcessGameServerData);
			networkManager->OnGameServerDataReceived.AddDynamic(this, &UMOBManager::ProcessGameServerData);
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

// Set world context 
void UMOBManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

// Process game server data
void UMOBManager::ProcessGameServerData(const FString& ReceivedData)
{
	//UE_LOG(LogTemp, Warning, TEXT("MOBManager received data: %s"), *ReceivedData);

	// Deserialize the received JSON string to get MessageData struct
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);

	// log the event type
	UE_LOG(LogTemp, Warning, TEXT("MOBManager: Received event type: %s"), *MessageData.eventType);

	//TODO : Move this to SpawnZoneManager
	if (MessageData.eventType == "spawnMobsInZone" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBManager: Received spawnMobsInZone event"));

		// Deserialize the JSON string into zone data
		//FSpawnZoneStruct SpawnZoneData = JSONParser::DeserializeSpawnZoneData(ReceivedData);

		// Deserialize the JSON string into an array of MOB structs
		TArray<FMOBStruct> MobsData = JSONParser::DeserializeMobsDataList(ReceivedData);

		// Spawn the mobs in the zone
		for (FMOBStruct MobData : MobsData)
		{
			SpawnMOB(MobData);
		}
	}	
	else if (MessageData.eventType == "getMOBData" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("Received getMOBData event"));

		// Deserialize the JSON string into an array of MOB structs
		FMOBStruct MobData = JSONParser::DeserializeMobData(ReceivedData);
	}
	else if (MessageData.eventType == "zoneMoveMobs" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("Received moveMOB event"));

		// Deserialize the JSON string into an array of MOB structs
		TArray<FMOBStruct> MobsData = JSONParser::DeserializeMobsDataList(ReceivedData);

		// Deserialize the received JSON string to get PositionData struct
		//FPositionDataStruct PositionData = JSONParser::DeserializePositionData(ReceivedData);

		// Move the MOBs in the zone
		for (FMOBStruct MobData : MobsData)
		{
			// If MOB exists in the world, move it
			if (MOBExists(worldContext, FName(MobData.mobUniqueID)))
			{
				// get the MOB
				TArray<AActor*> FoundActors;
				UGameplayStatics::GetAllActorsWithTag(worldContext, FName(MobData.mobUniqueID), FoundActors);

				// If the array is not empty, then actors with the tag exist
				if (FoundActors.Num() > 0)
				{
					ABasicMOB* MOB = Cast<ABasicMOB>(FoundActors[0]);

					// set new timestamp
					MOB->SetLastTimestamp(MessageData.timestamp);

					// set the MOB position
					MOB->SetMOBPosition(MobData.mobPosition);

					// set the MOB position
					//MOB->SetActorLocation(FVector(MobData.mobPosition.positionX, MobData.mobPosition.positionY, MobData.mobPosition.positionZ));
				}
			}
		}

	}
}

// Send join game request
void UMOBManager::SendGetMobData(const FClientDataStruct& ClientData)
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
	TSharedPtr<FJsonValueNumber> MOBID = MakeShareable(new FJsonValueNumber(1));
	BodyData.Add("mobId", MOBID);

	FString JsonString = JSONParser::SerializeJson("getMOBData", HeaderData, BodyData);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the game server
		networkManager->SendDataToGameServer(JsonString);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

// Spawn a MOB
void UMOBManager::SpawnMOB(const FMOBStruct& MOBData)
{
	// If MOB exists in the world do not spawn it
	if (MOBExists(worldContext, FName(MOBData.mobUniqueID)))
	{
		UE_LOG(LogTemp, Warning, TEXT("MOB already exists in the world"));
		return;
	}


	// get the game instance
	if (gameInstance)
	{
		// get basic MOB class
		TSubclassOf<class ABasicMOB> BasicMOBClass = gameInstance->GetBasicMOBClass();

		if (BasicMOBClass == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Basic MOB class not found"));
			return;
		}

		// spawn the MOB
		ABasicMOB* MOB = worldContext->SpawnActor<ABasicMOB>(
			BasicMOBClass,
			FVector(MOBData.mobPosition.positionX, 
				MOBData.mobPosition.positionY, 
				MOBData.mobPosition.positionZ), 
			FRotator(0, 0, 0)
		);

		if (MOB == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("MOB not spawned"));
			return;
		}

		// set the MOB data
		MOB->SetMOBData(MOBData);
		// set the MOB tag
		MOB->SetMOBTag(MOBData.mobUniqueID);
	}
}

// Check if a MOB exists in the world
bool UMOBManager::MOBExists(UWorld* World, const FName& Tag)
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(World, Tag, FoundActors);

	// If the array is not empty, then actors with the tag exist
	return FoundActors.Num() > 0;
}