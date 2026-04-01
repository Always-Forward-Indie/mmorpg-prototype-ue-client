// Fill out your copyright notice in the Description page of Project Settings.



#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "MyGameInstance.h"


UPlayerManager::UPlayerManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CombatNetworkHandler is now handled centrally by GameInstance
}

void UPlayerManager::Initialize(UNetworkManager* NetworkManager, UPingManager* PingManager)
{
	if (!NetworkManager || !PingManager)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerManager: Cannot initialize with null NetworkManager or PingManager"));
		return;
	}

	networkManager = NetworkManager;
	pingManager = PingManager;
	
	// Get the game instance safely
	if (worldContext && IsValid(worldContext))
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

		if (gameInstance && IsValid(gameInstance))
		{
			UE_LOG(LogTemp, Warning, TEXT("GameInstance found"));
			
			// Set up PingManager with TimeSyncService integration
			if (pingManager)
			{
				pingManager->SetTimeSyncService(gameInstance->GetTimeSyncService());
				UE_LOG(LogTemp, Warning, TEXT("PlayerManager: PingManager configured with TimeSyncService"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GameInstance not found or invalid"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerManager: WorldContext is null or invalid"));
	}
}

bool UPlayerManager::IsCombatEvent(const FString& EventType) const
{
	// Define combat-related events that should be handled by CombatNetworkHandler
	return EventType == TEXT("combatInitiation") || 
		   EventType == TEXT("combatResult") ||
		   EventType == TEXT("combatAnimation") ||
		   EventType == TEXT("initiateCombatAction") ||
		   EventType == TEXT("mobTargetLost"); // Add mobTargetLost as combat event
}

// subscribe to the network manager's event
void UPlayerManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Network Manager found and subscribed to GameServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogTemp, Warning, TEXT("Network Manager is valid"));

			// Subscribe to the network manager's events for game server
			networkManager->OnGameServerDataReceived.RemoveDynamic(this, &UPlayerManager::ProcessGameServerData);
			networkManager->OnGameServerDataReceived.AddDynamic(this, &UPlayerManager::ProcessGameServerData);

			// Subscribe to the network manager's events for chunk server data
			networkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UPlayerManager::ProcessChunkServerData);
			networkManager->OnChunkServerDataReceived.AddDynamic(this, &UPlayerManager::ProcessChunkServerData);
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
void UPlayerManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

// Process game server data
void UPlayerManager::ProcessGameServerData(const FString& ReceivedData)
{
	UE_LOG(LogTemp, Warning, TEXT("Init by delegate Game Server: %s"), *ReceivedData);

	// Deserialize the received JSON string to get MessageData struct
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	// Deserialize the received JSON string to get ClientData struct
	FClientDataStruct ClientData = JSONParser::DeserializeClientData(ReceivedData);


	// If the data is a response to a join game request on Game Server
	if (MessageData.eventType == "joinGameClient" && MessageData.status == "success" && ClientData.clientId != 0 && ClientData.hash != "") {
		if (gameInstance) {
			UE_LOG(LogTemp, Warning, TEXT("Joined Client to Game Server with ID: %d"), ClientData.clientId);

			// Phase 1: Register client session on Chunk Server
			SendJoinClientChunkRequest(ClientData);
		}
	}

	// If the data is a response to a leave game request
	if (MessageData.eventType == "disconnectClient" && ClientData.clientId != 0) {
		if (gameInstance) {
			// If the client ID is not the same as the current client ID
			if (gameInstance->GetCurrentClientID() != ClientData.clientId) {
				gameInstance->HandlePlayerDisconnection(ClientData.clientId);
			}
		}
	}
}

// Process chunk server data
void UPlayerManager::ProcessChunkServerData(const FString& ReceivedData)
{
	UE_LOG(LogTemp, Warning, TEXT("Init by delegate Chunk Server: %s"), *ReceivedData);

	// Проверяем валидность данных перед парсингом
	if (ReceivedData.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerManager: Received empty data"));
		return;
	}

	// Парсим JSON данные с проверкой валидности
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerManager: Failed to parse JSON data"));
		return;
	}

	// Безопасно получаем body
	TSharedPtr<FJsonObject> Body;
	if (Root->HasField(TEXT("body")))
	{
		Body = Root->GetObjectField(TEXT("body"));
		if (!Body.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerManager: Invalid body object"));
			return;
		}
	}

	// Deserialize the received JSON string to get MessageData struct
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	
	// Проверяем валидность MessageData
	if (MessageData.eventType.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerManager: Empty event type"));
		return;
	}

	// Deserialize the received JSON string to get ClientData struct
	FClientDataStruct ClientData = JSONParser::DeserializeClientData(ReceivedData);
	
	// Deserialize character data only if body and character field exist
	FCharacterDataStruct CharacterData;
	if (Body.IsValid() && Body->HasField(TEXT("character")))
	{
		TSharedPtr<FJsonObject> CharacterObject = Body->GetObjectField(TEXT("character"));
		if (CharacterObject.IsValid())
		{
			CharacterData = JSONParser::DeserializeCharacterData(CharacterObject);
		}
	}

	//if character data is valid
	if (CharacterData.characterId != 0) {
		// Set the character data
		ClientData.characterData = CharacterData;
	}

	// Phase 1 response: joinGameClient on Chunk Server - now send joinGameCharacter (Phase 1 continued)
	if (MessageData.eventType == "joinGameClient" && MessageData.status == "success" && ClientData.clientId != 0) {
		if (gameInstance && IsValid(gameInstance)) {
			UE_LOG(LogTemp, Warning, TEXT("Registered Client on Chunk Server with ID: %d, sending joinGameCharacter"), ClientData.clientId);
			SendJoinCharacterChunkRequest(ClientData);
		}
	}

	// Phase 3: playerReady ACK from server
	if (MessageData.eventType == "playerReady" && MessageData.status == "success") {
		if (gameInstance && IsValid(gameInstance)) {
			UE_LOG(LogTemp, Warning, TEXT("Server acknowledged playerReady - Phase 4 world-state will arrive automatically"));
			// Server will now auto-send: spawnNPCs, spawnMobsInZone, nearbyItems, PLAYER_EQUIPMENT_UPDATE
			// Signal the loading screen gate: Phase 4 has started, start the grace timer.
			gameInstance->NotifyPlayerReadyAck();
		}
	}

	// Handle positionCorrection (anti-cheat server correction)
	if (MessageData.eventType == "positionCorrection") {
		if (gameInstance && IsValid(gameInstance) && gameInstance->IsGameWorldReady()) {
			if (Body.IsValid() && Body->HasField(TEXT("position"))) {
				TSharedPtr<FJsonObject> PosObj = Body->GetObjectField(TEXT("position"));
				if (PosObj.IsValid()) {
					double CorrX = PosObj->GetNumberField(TEXT("x"));
					double CorrY = PosObj->GetNumberField(TEXT("y"));
					double CorrZ = PosObj->GetNumberField(TEXT("z"));
					double CorrRotZ = PosObj->GetNumberField(TEXT("rotationZ"));
					UE_LOG(LogTemp, Warning, TEXT("Position correction received: (%.1f, %.1f, %.1f) rot=%.2f"), CorrX, CorrY, CorrZ, CorrRotZ);
					gameInstance->UpdatePlayerCoordinates(gameInstance->GetCurrentClientID(), CorrX, CorrY, CorrZ, CorrRotZ);
				}
			}
		}
	}

	// Phase 1 continued: joinGameCharacter response
	if (MessageData.eventType == "joinGameCharacter" && MessageData.status == "success" && ClientData.clientId != 0) {
		if (gameInstance && IsValid(gameInstance)) {
			UE_LOG(LogTemp, Warning, TEXT("Joined Character to Chunk Server with ID: %d, Character ID: %d"), ClientData.clientId, ClientData.characterData.characterId);

			// If the client ID is the same as the current client ID
			if (gameInstance->GetCurrentClientID() == ClientData.clientId) {
				// Store player data before world transition
				gameInstance->AddPlayerData(ClientData.clientId, ClientData);
				// Queue local player spawn for after the game world loads
				gameInstance->PendingSpawnClientId = ClientData.clientId;
				// Transition to game world (OpenLevel for World Partition)
				gameInstance->TransitionToGameWorld();
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("Init Position of other Player: X: %f, Y: %f, Z: %f"), ClientData.characterData.characterPosition.positionX, ClientData.characterData.characterPosition.positionY, ClientData.characterData.characterPosition.positionZ);
				// If the game world is ready, spawn immediately; otherwise queue for later
				if (gameInstance->IsGameWorldReady())
				{
					gameInstance->AddPlayerData(ClientData.clientId, ClientData);
					gameInstance->SpawnPlayerForClient(ClientData.clientId);
				}
				else
				{
					gameInstance->PendingRemotePlayerSpawns.Add(ClientData);
					UE_LOG(LogTemp, Warning, TEXT("PlayerManager: Queued remote player spawn (world not ready yet)"));
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerManager: GameInstance is null or invalid in joinGameCharacter"));
		}
	}

	// If the data is a response to a get connected players request
	if (MessageData.eventType == "getConnectedCharacters" && MessageData.status == "success" && ClientData.clientId != 0) {
		if (gameInstance && IsValid(gameInstance)) {
			TArray<FClientDataStruct> ConnectedPlayers = JSONParser::DeserializeCharactersList(Body);

			for (const FClientDataStruct& ConnectedPlayer : ConnectedPlayers) {
				if (gameInstance->GetCurrentClientID() != ConnectedPlayer.clientId) {
					UE_LOG(LogTemp, Warning, TEXT("Connected Player Client ID: %d, Current Client ID: %d"), ConnectedPlayer.clientId, gameInstance->GetCurrentClientID());

					if (gameInstance->IsGameWorldReady())
					{
						gameInstance->AddPlayerData(ConnectedPlayer.clientId, ConnectedPlayer);
						gameInstance->SpawnPlayerForClient(ConnectedPlayer.clientId);
					}
					else
					{
						gameInstance->PendingRemotePlayerSpawns.Add(ConnectedPlayer);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerManager: GameInstance is null or invalid in getConnectedCharacters"));
		}
	}

	// If the data is a response to a move player request
	if (MessageData.eventType == "moveCharacter" && MessageData.status == "success" && CharacterData.characterId != 0) {
		if (gameInstance && IsValid(gameInstance) && gameInstance->IsGameWorldReady()) {
			if (gameInstance->GetCurrentClientID() != ClientData.clientId) {
				gameInstance->MovePlayerForClient(ClientData.clientId, ClientData, MessageData);
			}
		}
	}

	// If the data is a response to a leave game request
	// Per protocol, disconnectClient puts clientId and characterId in body
	if (MessageData.eventType == "disconnectClient") {
		int32 DisconnectedClientId = ClientData.clientId;
		// Also check body for clientId (protocol puts it there)
		if (DisconnectedClientId == 0 && Body.IsValid() && Body->HasField(TEXT("clientId"))) {
			DisconnectedClientId = Body->GetIntegerField(TEXT("clientId"));
		}
		if (DisconnectedClientId != 0 && gameInstance && IsValid(gameInstance)) {
			if (gameInstance->GetCurrentClientID() != DisconnectedClientId) {
				gameInstance->HandlePlayerDisconnection(DisconnectedClientId);
			}
		}
		else if (!gameInstance || !IsValid(gameInstance))
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerManager: GameInstance is null or invalid in disconnectClient"));
		}
	}

	// Handle experience_update (XP gain, level up)
	if (MessageData.eventType == "experience_update" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerManager: Received experience_update"));
		FExperienceUpdateStruct ExpUpdate = JSONParser::DeserializeExperienceUpdate(ReceivedData);

		if (gameInstance && IsValid(gameInstance) && ExpUpdate.characterId > 0)
		{
			UExperienceManager* ExpMgr = gameInstance->GetExperienceManager();
			if (ExpMgr && IsValid(ExpMgr))
			{
				ExpMgr->ProcessExperienceUpdate(ExpUpdate);
				UE_LOG(LogTemp, Log, TEXT("PlayerManager: Processed experience_update for character %d (+%d XP, Level %d->%d, LevelUp=%s)"),
					ExpUpdate.characterId, ExpUpdate.experienceChange,
					ExpUpdate.oldLevel, ExpUpdate.newLevel,
					ExpUpdate.levelUp ? TEXT("true") : TEXT("false"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("PlayerManager: ExperienceManager not available for experience_update"));
			}
		}
	}

	// Handle player stats update
	if (MessageData.eventType == "stats_update" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerManager: Received stats_update"));
		FPlayerStatsUpdateStruct StatsUpdate = JSONParser::DeserializePlayerStatsUpdate(ReceivedData);
		
		if (gameInstance && IsValid(gameInstance))
		{
			// Get the player for this character ID
			int32 CharacterId = StatsUpdate.characterId;
			if (CharacterId > 0)
			{
				// Find and update the player
				ABasicPlayer* Player = gameInstance->GetPlayerByCharacterId(CharacterId);
				if (Player && IsValid(Player))
				{
					Player->ProcessStatsUpdate(StatsUpdate);
					UE_LOG(LogTemp, Log, TEXT("PlayerManager: Updated stats for character %d"), CharacterId);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("PlayerManager: Player not found for character ID %d"), CharacterId);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("PlayerManager: Invalid character ID in stats update: %d"), CharacterId);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerManager: GameInstance is null or invalid in stats_update"));
		}
	}

	// Handle respawnResult - server response to respawnRequest
	if (MessageData.eventType == "respawnResult" && MessageData.status == "success")
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerManager: Received respawnResult"));

		if (gameInstance && IsValid(gameInstance) && Body.IsValid())
		{
			int32 RespawnCharId = 0;
			Body->TryGetNumberField(TEXT("characterId"), RespawnCharId);

			if (RespawnCharId <= 0)
			{
				// Fallback: use local character ID (server may omit it for the requesting client)
				RespawnCharId = gameInstance->GetCurrentCharacterID();
			}

			ABasicPlayer* RespawnPlayer = gameInstance->GetPlayerByCharacterId(RespawnCharId);
			if (RespawnPlayer && IsValid(RespawnPlayer))
			{
				// Parse respawn position
				const TSharedPtr<FJsonObject>* PosObj = nullptr;
				if (Body->TryGetObjectField(TEXT("position"), PosObj) && PosObj)
				{
					double RX = 0, RY = 0, RZ = 0, RRot = 0;
					(*PosObj)->TryGetNumberField(TEXT("x"), RX);
					(*PosObj)->TryGetNumberField(TEXT("y"), RY);
					(*PosObj)->TryGetNumberField(TEXT("z"), RZ);
					(*PosObj)->TryGetNumberField(TEXT("rotationZ"), RRot);

					// Teleport the actor immediately (no interpolation for respawn) and
					// sync playerData so subsequent movement packets use the correct origin.
					// SetCoordinates only updates the interpolation targets; we also call
					// SetActorLocation/Rotation so the actor snaps to the respawn point
					// before movement is re-enabled by SetDead_Implementation(false).
					RespawnPlayer->SetActorLocation(FVector(RX, RY, RZ), false, nullptr, ETeleportType::TeleportPhysics);
					RespawnPlayer->SetActorRotation(FRotator(0.0, RRot, 0.0));
					RespawnPlayer->SetCoordinates(RX, RY, RZ, RRot);
				}

				// Revive the player: restores movement mode, hides death screen,
				// plays revive sound, notifies AnimBP. Must happen AFTER teleport so
				// CMC re-enables walking at the correct world position.
				RespawnPlayer->SetDead_Implementation(false);

				UE_LOG(LogTemp, Warning, TEXT("PlayerManager: Character %d respawned successfully"), RespawnCharId);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("PlayerManager: Could not find player for respawnResult (charId=%d)"), RespawnCharId);
			}
		}
	}
}

void UPlayerManager::StartPing()
{
	if (worldContext && pingManager)
	{
		// Set world context for PingManager
		pingManager->SetWorldContext(worldContext);
		
		// Start the new TimeSyncService-based ping updates
		pingManager->StartPingUpdates();
		
		UE_LOG(LogTemp, Warning, TEXT("PlayerManager: Started TimeSyncService-based ping updates"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerManager: Cannot start ping - missing worldContext or pingManager"));
	}
}

void UPlayerManager::SendJoinGameRequest(const FClientDataStruct& ClientData)
{
	// PRE-PHASE: Send joinGameClient to Game Server with characterId
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add the client ID and hash to the header data
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	// Add the Character ID to the body data (required per protocol §1.0)
	int32 CharId = ClientData.characterData.characterId;
	if (gameInstance && gameInstance->GetCurrentCharacterID() != 0) {
		CharId = gameInstance->GetCurrentCharacterID();
	}
	BodyData.Add("characterId", MakeShareable(new FJsonValueNumber(CharId)));

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("joinGameClient", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::GameServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the game server
		networkManager->SendDataToGameServer(JsonString);
		UE_LOG(LogTemp, Warning, TEXT("PRE-PHASE: joinGameClient sent to Game Server for CharID=%d"), CharId);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void UPlayerManager::SendJoinClientChunkRequest(const FClientDataStruct& ClientData)
{
	// Phase 1: Register client session on Chunk Server
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add the client ID and hash to the header data per protocol
	HeaderData.Add("clientId", MakeShareable(new FJsonValueNumber(ClientData.clientId)));
	HeaderData.Add("hash", MakeShareable(new FJsonValueString(ClientData.hash)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync("joinGameClient", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		networkManager->SendDataToChunkServer(JsonString);
		UE_LOG(LogTemp, Warning, TEXT("Phase 1: joinGameClient sent to Chunk Server"));
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void UPlayerManager::SendPlayerReadyRequest(const FClientDataStruct& ClientData)
{
	// Phase 3: Send playerReady ACK after game world is fully loaded
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Per protocol: header has clientId and hash
	HeaderData.Add("clientId", MakeShareable(new FJsonValueNumber(ClientData.clientId)));
	HeaderData.Add("hash", MakeShareable(new FJsonValueString(ClientData.hash)));

	// Per protocol: body has characterId
	int32 CharId = ClientData.characterData.characterId;
	if (gameInstance && gameInstance->GetCurrentCharacterID() != 0) {
		CharId = gameInstance->GetCurrentCharacterID();
	}
	BodyData.Add("characterId", MakeShareable(new FJsonValueNumber(CharId)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync("playerReady", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		networkManager->SendDataToChunkServer(JsonString);
		UE_LOG(LogTemp, Warning, TEXT("Phase 3: playerReady sent to Chunk Server for CharID=%d"), CharId);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void UPlayerManager::SendJoinCharacterChunkRequest(const FClientDataStruct& ClientData)
{
	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add the client ID and hash to the header data
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	// Add the Character ID to the body data
	TSharedPtr<FJsonValueNumber> CharacterIDValue = MakeShareable(new FJsonValueNumber(ClientData.characterData.characterId));
	
	CharacterIDValue = gameInstance->GetCurrentCharacterID() != 0 ? MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentCharacterID())) : CharacterIDValue;
	BodyData.Add("id", CharacterIDValue);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("joinGameCharacter", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the Chunk server
		networkManager->SendDataToChunkServer(JsonString);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void UPlayerManager::SendGetConnectedPlayersRequest(FClientDataStruct& ClientData)
{
	if (!networkManager || !IsValid(networkManager))
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerManager: NetworkManager is null or invalid"));
		return;
	}

	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add the client ID and hash to the header data
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	// Add character id to the body data
	TSharedPtr<FJsonValueNumber> CharacterIDValue = MakeShareable(new FJsonValueNumber(ClientData.characterData.characterId));

	BodyData.Add("characterId", CharacterIDValue);

	// Get Connected Characters - это ChunkServer
	FString getConnectedCharacters = JSONParser::SerializeJsonWithTimeSync("getConnectedCharacters", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	// Get spawn zones and spawn mobs - это тоже ChunkServer
	FString getSpawnZones = JSONParser::SerializeJsonWithTimeSync("getSpawnZones", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	// Validate JSON strings before sending
	if (getConnectedCharacters.IsEmpty() || getSpawnZones.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerManager: Failed to serialize JSON data"));
		return;
	}

	// Send the JSON string to the chunk server
	networkManager->SendDataToChunkServer(getConnectedCharacters);
	networkManager->SendDataToChunkServer(getSpawnZones);
}

void UPlayerManager::SendMovePlayerRequest(FClientDataStruct& ClientData)
{
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Header: clientId + hash (server identifies player via clientId)
	HeaderData.Add("clientId", MakeShareable(new FJsonValueNumber(ClientData.clientId)));
	HeaderData.Add("hash", MakeShareable(new FJsonValueString(ClientData.hash)));

	// Body: position data only per protocol (posX, posY, posZ, rotZ)
	BodyData.Add("posX", MakeShareable(new FJsonValueNumber(ClientData.characterData.characterPosition.positionX)));
	BodyData.Add("posY", MakeShareable(new FJsonValueNumber(ClientData.characterData.characterPosition.positionY)));
	BodyData.Add("posZ", MakeShareable(new FJsonValueNumber(ClientData.characterData.characterPosition.positionZ)));
	BodyData.Add("rotZ", MakeShareable(new FJsonValueNumber(ClientData.characterData.characterPosition.rotationZ)));

	UTimeSyncService* TimeSyncService = gameInstance ? gameInstance->GetTimeSyncService() : nullptr;

	FString JsonString = JSONParser::SerializeJsonWithTimeSync("moveCharacter", HeaderData, BodyData, TimeSyncService, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		networkManager->SendDataToChunkServer(JsonString);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void UPlayerManager::SendLeaveGameRequest(FClientDataStruct& ClientData)
{
	// Create a JSON object for the header and body data
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add the client ID and hash to the header data
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	// Add character id to the body data
	TSharedPtr<FJsonValueNumber> CharacterIDValue = MakeShareable(new FJsonValueNumber(ClientData.characterData.characterId));

	BodyData.Add("characterId", CharacterIDValue);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("disconnectClient", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		// Notify Game Server and Chunk Server only.
		// Login Server disconnect is sent separately by AuthenticationManager::SendLeaveGameRequest
		// to avoid sending duplicate disconnectClient packets to the Login Server.
		//networkManager->SendDataToGameServer(JsonString);
		networkManager->SendDataToChunkServer(JsonString);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void UPlayerManager::SendPlayerAttackRequest(const FClientDataStruct& ClientData, int32 TargetID, const FString& SkillSlug, int32 TargetTypeId)
{
	// Create JSON data for the attack request
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Add client authentication to header
	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	// Add attack data to body
	TSharedPtr<FJsonValueString> SkillSlugValue = MakeShareable(new FJsonValueString(SkillSlug));
	TSharedPtr<FJsonValueNumber> TargetIDValue = MakeShareable(new FJsonValueNumber(TargetID));
	TSharedPtr<FJsonValueNumber> TargetTypeIdValue = MakeShareable(new FJsonValueNumber(TargetTypeId));

	BodyData.Add("skillSlug", SkillSlugValue);
	BodyData.Add("targetId", TargetIDValue);
	BodyData.Add("targetType", TargetTypeIdValue);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("playerAttack", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		// Send the attack request to the Chunk server
		networkManager->SendDataToChunkServer(JsonString);
		UE_LOG(LogTemp, Warning, TEXT("Attack request sent: %s"), *JsonString);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NetworkManager is null, cannot send attack request"));
	}
}

void UPlayerManager::SendRespawnRequest(const FClientDataStruct& ClientData)
{
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	TSharedPtr<FJsonValueNumber> ClientIDValue = MakeShareable(new FJsonValueNumber(ClientData.clientId));
	TSharedPtr<FJsonValueString> HashValue = MakeShareable(new FJsonValueString(ClientData.hash));

	HeaderData.Add("clientId", ClientIDValue);
	HeaderData.Add("hash", HashValue);

	BodyData.Add("characterId", MakeShareable(new FJsonValueNumber(ClientData.characterData.characterId)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync("respawnRequest", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		networkManager->SendDataToChunkServer(JsonString);
		UE_LOG(LogTemp, Warning, TEXT("Respawn request sent: %s"), *JsonString);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NetworkManager is null, cannot send respawn request"));
	}
}
