// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"

//constructor
UMOBManager::UMOBManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UMOBManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;

	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());
	}

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

		if (!worldContext || !worldContext->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Error, TEXT("MOBManager: Cannot process zoneMoveMobs - no valid world context"));
			return;
		}

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
		int32 MobUID = 0;
		if (!Body.IsValid() || !Body->TryGetNumberField(TEXT("mobUID"), MobUID) || MobUID <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("MOBManager: mobDeath - missing or invalid mobUID"));
			return;
		}

		TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(MobUID);
		if (!FoundWeak || !FoundWeak->IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("MOBManager: mobDeath for UID %d - actor not found (already cleaned up?)"), MobUID);
			return;
		}

		ABasicMOB* MOB = FoundWeak->Get();
		if (!IsValid(MOB))
		{
			UE_LOG(LogTemp, Warning, TEXT("MOBManager: mobDeath for UID %d - actor is pending kill"), MobUID);
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("MOBManager: mobDeath for UID %d (%s)"), MobUID, *MOB->GetMobName());

		if (!MOB->GetMOBIsDead())
		{
			MOB->Die();
		}

		MOB->Destroy();
	}
	else if (MessageData.eventType == "mobMoveUpdate" && MessageData.status == "success")
	{
		if (!worldContext || !worldContext->IsValidLowLevel()) return;

		int64 ServerSendMs = 0;
		{
			const TSharedPtr<FJsonObject>* HdrPtr = nullptr;
			if (Root->TryGetObjectField(TEXT("header"), HdrPtr) && HdrPtr)
			{
				double RawMs = 0.0;
				if ((*HdrPtr)->TryGetNumberField(TEXT("serverSendMs"), RawMs))
				{
					ServerSendMs = static_cast<int64>(RawMs);
				}
			}
		}

		const FDateTime UtcNow    = FDateTime::UtcNow();
		const int64 ClientRecvMs  = UtcNow.ToUnixTimestamp() * 1000 + UtcNow.GetMillisecond();

		TArray<JSONParser::FMobMovePacketEntry> Entries =
			JSONParser::DeserializeMobMoveUpdate(Body, ServerSendMs);

		for (const JSONParser::FMobMovePacketEntry& Entry : Entries)
		{
			TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(Entry.uid);
			if (!FoundWeak || !FoundWeak->IsValid()) continue;

			ABasicMOB* MOB = FoundWeak->Get();
			if (IsValid(MOB))
			{
				MOB->OnReceiveMovePacket(Entry.moveEntry, ServerSendMs, ClientRecvMs);
			}
		}
	}
	else if (MessageData.eventType == "mobHealthUpdate")
	{
		UE_LOG(LogTemp, Log, TEXT("MOBManager: Received mobHealthUpdate event"));

		if (!worldContext || !worldContext->IsValidLowLevel()) return;

		int32 MobUID = 0;
		int32 CurrentHP = 0;
		int32 MaxHP = 0;
		if (Body.IsValid())
		{
			Body->TryGetNumberField(TEXT("mobUID"), MobUID);
			Body->TryGetNumberField(TEXT("currentHealth"), CurrentHP);
			Body->TryGetNumberField(TEXT("maxHealth"), MaxHP);
		}

		if (MobUID > 0)
		{
			TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(MobUID);
			if (FoundWeak && FoundWeak->IsValid())
			{
				ABasicMOB* MOB = FoundWeak->Get();
				FMobHealthUpdateStruct HealthUpdate;
				HealthUpdate.mobUID = MobUID;
				HealthUpdate.mobId  = Body.IsValid() ? Body->GetIntegerField(TEXT("mobId")) : 0;
				HealthUpdate.currentHealth = CurrentHP;
				HealthUpdate.maxHealth = MaxHP;
				MOB->OnReceiveMobHealthUpdate(HealthUpdate);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("MOBManager: Mob UID %d not found in registry for mobHealthUpdate"), MobUID);
			}
		}
	}
	else if (MessageData.eventType == "mobTargetLost" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBManager: Received mob target lost event"));

		if (!worldContext || !worldContext->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Error, TEXT("MOBManager: Cannot process mobTargetLost - no valid world context"));
			return;
		}

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
						// ���������� ���� ����
						MOB->SetMobTargetId(0);
						MOB->SetMobTargetType("");
						MOB->SetMOBIsAggressive(false);

						// ��������� ������� ����
						MOB->OnReceiveServerPacket(TargetLostData.position);

						// Notify the mob (animation transition, delegate broadcast)
						MOB->OnReceiveTargetLost();

						// If the local player had this mob locked, clear the lock target
						if (worldContext)
						{
							APlayerController* PC = worldContext->GetFirstPlayerController();
							if (PC)
							{
								ABasicPlayer* LocalPlayer = Cast<ABasicPlayer>(PC->GetPawn());
								if (LocalPlayer && LocalPlayer->GetLockedTarget() == MOB)
								{
									LocalPlayer->ClearLockedTarget();
									UE_LOG(LogTemp, Log, TEXT("MOBManager: Cleared player lock target � mob %d returned to zone"),
										TargetLostData.mobUID);
								}
							}
						}

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
	else if (MessageData.eventType == "effectTick")
	{
		// Route effectTick to the target mob so its HP bar and visual feedback updates.
		// Player effectTick is already handled by PlayerStatsNetworkHandler; here we only
		// forward to mobs whose characterId matches a mob UID in the registry.
		if (!worldContext || !worldContext->IsValidLowLevel()) return;

		FEffectTickData TickData = JSONParser::DeserializeEffectTick(ReceivedData);
		if (TickData.characterId > 0)
		{
			TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(TickData.characterId);
			if (FoundWeak && FoundWeak->IsValid())
			{
				FoundWeak->Get()->OnReceiveEffectTick(TickData);
			}
		}
	}
	else if (MessageData.eventType == "corpseRemoved")
	{
		if (!worldContext || !worldContext->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Error, TEXT("MOBManager: Cannot process corpseRemoved - no valid world context"));
			return;
		}

		if (!Body.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("MOBManager: corpseRemoved packet has no body"));
			return;
		}

		int32 CorpseUID = 0;
		if (!Body->TryGetNumberField(TEXT("corpseUID"), CorpseUID) || CorpseUID <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("MOBManager: corpseRemoved - missing or invalid corpseUID"));
			return;
		}

		TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(CorpseUID);
		if (!FoundWeak || !FoundWeak->IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("MOBManager: corpseRemoved for UID %d - actor not found (already cleaned up?)"), CorpseUID);
			return;
		}

		ABasicMOB* MOB = FoundWeak->Get();
		if (!IsValid(MOB))
		{
			UE_LOG(LogTemp, Warning, TEXT("MOBManager: corpseRemoved for UID %d - actor is pending kill"), CorpseUID);
			return;
		}

		if (!MOB->GetMOBIsDead())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("MOBManager: corpseRemoved for UID %d (%s) - mob is NOT marked dead on client. Server-authoritative removal applied."),
				CorpseUID, *MOB->GetMobName());
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("MOBManager: Removing corpse UID %d (%s)"), CorpseUID, *MOB->GetMobName());
		}

		MOB->Destroy();
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
	if (!worldContext || !worldContext->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("MOBManager: Cannot update mob health - no valid world context"));
		return;
	}

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
	if (!worldContext || !worldContext->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("MOBManager: Cannot spawn MOB - no valid world context"));
		return;
	}

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

		// Always seed the movement component so bHasReceivedPacket is true
		// before the first mobMoveUpdate arrives.  Without this, the first
		// packet hits the "first packet ever" branch and hard-teleports the
		// mob instead of smoothly interpolating.
		if (MOB->MOBMovementComponent)
		{
			const FVector VelDir(MOBData.mobVelocity.dirX, MOBData.mobVelocity.dirY, 0.f);
			const FVector SpawnPos(MOBData.mobPosition.positionX,
								   MOBData.mobPosition.positionY,
								   MOB->GetActorLocation().Z);
			MOB->MOBMovementComponent->InitializeFromSpawnData(
				SpawnPos, VelDir, MOBData.mobVelocity.speed, MOBData.mobCombatState);
		}
	}
}

// Check if a MOB exists in the world
bool UMOBManager::MOBExists(UWorld* World, const FName& Tag)
{
	if (!World || !World->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("MOBManager::MOBExists: Invalid world context"));
		return false;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(World, Tag, FoundActors);

	// If the array is not empty, then actors with the tag exist
	return FoundActors.Num() > 0;
}

AActor* UMOBManager::FindMobActor(int32 MobUID) const
{
	const TWeakObjectPtr<ABasicMOB>* Found = MobActorRegistry.Find(MobUID);
	if (Found && Found->IsValid())
	{
		return Found->Get();
	}
	// Fallback: tag-based search
	if (!worldContext) return nullptr;
	const FName UIDTag = FName(*FString::FromInt(MobUID));
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(worldContext, UIDTag, FoundActors);
	return FoundActors.Num() > 0 ? FoundActors[0] : nullptr;
}

void UMOBManager::RegisterMob(int32 MobUID, ABasicMOB* MobActor)
{
	if (MobUID > 0 && IsValid(MobActor))
	{
		MobActorRegistry.Add(MobUID, MobActor);
	}
}

void UMOBManager::UnregisterMob(int32 MobUID)
{
	MobActorRegistry.Remove(MobUID);
}

void UMOBManager::RegisterPlayer(int32 PlayerId, ABasicPlayer* PlayerActor)
{
	if (PlayerId > 0 && IsValid(PlayerActor))
	{
		PlayerRegistry.Add(PlayerId, PlayerActor);
	}
}

void UMOBManager::UnregisterPlayer(int32 PlayerId)
{
	PlayerRegistry.Remove(PlayerId);
}

void UMOBManager::ClearWorldState()
{
	MobActorRegistry.Empty();
	PlayerRegistry.Empty();
}