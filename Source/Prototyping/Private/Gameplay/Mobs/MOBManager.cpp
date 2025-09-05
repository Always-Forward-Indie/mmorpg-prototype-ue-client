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
		UE_LOG(LogTemp, Warning, TEXT("Network Manager found and subscribed to ChunkServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("Network Manager is valid"));

			// Subscribe to the network manager's events for chunk server data
			networkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UMOBManager::ProcessGameServerData);
			networkManager->OnChunkServerDataReceived.AddDynamic(this, &UMOBManager::ProcessGameServerData);
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

// Check if event is combat-related and should be handled by CombatNetworkHandler
bool UMOBManager::IsCombatEvent(const FString& EventType) const
{
	// Define combat-related events that should be handled by CombatNetworkHandler
	return EventType == TEXT("combatInitiation") || 
		   EventType == TEXT("combatResult") ||
		   EventType == TEXT("combatAnimation") ||
		   EventType == TEXT("initiateCombatAction") ||
		   EventType == TEXT("mobTargetLost"); // Add mobTargetLost as combat event
}

// Process game server data
void UMOBManager::ProcessGameServerData(const FString& ReceivedData)
{
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	UE_LOG(LogTemp, Warning, TEXT("MOBManager: Received event type: %s"), *MessageData.eventType);

	// Check if this is a combat event - if so, let CombatNetworkHandler handle it
	//if (IsCombatEvent(MessageData.eventType))
	//{
	//	UE_LOG(LogTemp, Log, TEXT("MOBManager: Delegating combat event '%s' to CombatNetworkHandler"), *MessageData.eventType);
	//	// Combat events are handled by CombatNetworkHandler which is already subscribed to the same event
	//	// No need to process them here
	//	return;
	//}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));

	if (MessageData.eventType == "spawnMobsInZone" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBManager: Received spawnMobsInZone event"));
		TArray<FMOBStruct> MobsData = JSONParser::DeserializeMobsList(Body);
		for (FMOBStruct MobData : MobsData)
		{
			SpawnMOB(MobData);
		}
	}
	else if (MessageData.eventType == "getMOBData" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("Received getMOBData event"));
		// Deserialize the JSON string into a single MOB data
		FMOBStruct MobData = JSONParser::DeserializeMobData(Body->GetObjectField(TEXT("mob")));
	}
	else if (MessageData.eventType == "zoneMoveMobs" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("Received moveMOB event"));

		// Deserialize the JSON string into a list of MOBs
		TArray<FMOBStruct> MobsData = JSONParser::DeserializeMobsList(Body);
		for (FMOBStruct MobData : MobsData)
		{
			if (MOBExists(worldContext, FName(MobData.mobUniqueID)))
			{
				TArray<AActor*> FoundActors;
				UGameplayStatics::GetAllActorsWithTag(worldContext, FName(MobData.mobUniqueID), FoundActors);

				if (FoundActors.Num() > 0)
				{
					ABasicMOB* MOB = Cast<ABasicMOB>(FoundActors[0]);

					MOB->OnReceiveServerPacket(MobData.mobPosition);
				}
			}
		}
	}
	else if (MessageData.eventType == "mobDeath" && MessageData.status == "success")
	{
		// Extract the mobUID from the response body
		FString MobUID;
		if (Body->TryGetStringField(TEXT("mobUID"), MobUID))
		{
			UE_LOG(LogTemp, Warning, TEXT("Received death event for mob: %s"), *MobUID);

			// Find and destroy the mob
			if (MOBExists(worldContext, FName(MobUID)))
			{
				TArray<AActor*> FoundActors;
				UGameplayStatics::GetAllActorsWithTag(worldContext, FName(MobUID), FoundActors);

				if (FoundActors.Num() > 0)
				{
					ABasicMOB* MOB = Cast<ABasicMOB>(FoundActors[0]);
					if (MOB)
					{
						UE_LOG(LogTemp, Warning, TEXT("Destroying mob: %s (%s)"),
							*MOB->GetMobName(), *MobUID);

						// Destroy the actor
						MOB->Destroy();
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Received death event for mob %s but it doesn't exist in the world"),
					*MobUID);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to extract mobUID from death event"));
		}
	}
	else if (MessageData.eventType == "mobTargetLost" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBManager: Received mob target lost event"));

		FMobTargetLostStruct TargetLostData = JSONParser::DeserializeMobTargetLost(Body);

		UE_LOG(LogTemp, Warning, TEXT("Mob %d (UID: %d) lost target player %d at position (%.2f, %.2f, %.2f)"),
			TargetLostData.mobId, TargetLostData.mobUID, TargetLostData.lostTargetPlayerId,
			TargetLostData.position.positionX, TargetLostData.position.positionY, TargetLostData.position.positionZ);

		FString MobUidStr = FString::FromInt(TargetLostData.mobUID);

		if (MOBExists(worldContext, FName(MobUidStr)))
		{
			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsWithTag(worldContext, FName(MobUidStr), FoundActors);

			if (FoundActors.Num() > 0)
			{
				ABasicMOB* MOB = Cast<ABasicMOB>(FoundActors[0]);
				if (MOB)
				{
					// —брасываем цель моба
					MOB->SetMobTargetId(0);
					MOB->SetMobTargetType("");
					MOB->SetMOBIsAggressive(false);

					// ќбновл€ем позицию моба
					MOB->OnReceiveServerPacket(TargetLostData.position);

					UE_LOG(LogTemp, Warning, TEXT("MOB %s (UID: %d) target cleared and position updated"),
						*MOB->GetMobName(), TargetLostData.mobUID);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Mob with UID %d not found for target lost event"), TargetLostData.mobUID);
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

// Update mob health based on combat result
void UMOBManager::UpdateMobHealth(const FCombatResultData& ResultData)
{
	// Find the mob in the world by ID
	FString MobUidStr = FString::FromInt(ResultData.TargetId);
	
	if (MOBExists(worldContext, FName(MobUidStr)))
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(worldContext, FName(MobUidStr), FoundActors);

		if (FoundActors.Num() > 0)
		{
			ABasicMOB* MOB = Cast<ABasicMOB>(FoundActors[0]);
			if (MOB)
			{
				// Update the mob's health
				MOB->SetMOBCurrentHealth(ResultData.RemainingHealth);
				
				// Set the damaged flag
				MOB->SetMobIsDamaged(ResultData.bIsDamaged);
				
				// If the mob died, handle death
				if (ResultData.bTargetDied)
				{
					MOB->SetMOBIsDead(true);
					
					// Trigger visual death animation/effect
					MOB->Die();
				}
				
				// Force update UI to show new health
				MOB->ForceUpdateUI();
				
				// Notify game instance about mob health update
				if (gameInstance)
				{
					gameInstance->UpdateMobHealth(
						ResultData.TargetId,
						ResultData.RemainingHealth,
						ResultData.RemainingMana,
						ResultData.bTargetDied,
						ResultData.bIsDamaged,
						ResultData.DamageDealt
					);
				}
				
				UE_LOG(LogTemp, Warning, TEXT("MOB %s health updated: %d, IsDead: %s"),
					*MOB->GetMobName(), MOB->GetMOBCurrentHealth(), ResultData.bTargetDied ? TEXT("True") : TEXT("False"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Mob with ID %d not found for health update"), ResultData.TargetId);
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

		MOB->SetupMobVisual(FName(MOBData.mobSlug));
		MOB->SetupMobAudio(FName(MOBData.mobSlug));

		// set the MOB data
		MOB->SetMOBData(MOBData);
		// set the MOB tag
		MOB->SetMOBTag(MOBData.mobUniqueID);
		MOB->SetMOBTag("Mob");
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