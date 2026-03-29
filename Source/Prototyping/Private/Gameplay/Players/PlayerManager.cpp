// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
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


	// If the data is a response to a join game request
	if (MessageData.eventType == "joinGameClient" && MessageData.status == "success" && ClientData.clientId != 0 && ClientData.hash != "") {
		if (gameInstance) {
			//ue log with client and character id
			UE_LOG(LogTemp, Warning, TEXT("Joined Client to Game Server with ID: %d, Character ID: %d"), ClientData.clientId, ClientData.characterData.characterId);

			SendJoinCharacterChunkRequest(ClientData);
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

	// If the data is a response to a join game request
	if (MessageData.eventType == "joinGameCharacter" && MessageData.status == "success" && ClientData.clientId != 0) {
		if (gameInstance && IsValid(gameInstance)) {
			//ue log with client and character id
			UE_LOG(LogTemp, Warning, TEXT("Joined Client to Chunk Server with ID: %d, Character ID: %d"), ClientData.clientId, ClientData.characterData.characterId);

			// If the client ID is the same as the current client ID
			if (gameInstance->GetCurrentClientID() == ClientData.clientId) {
				// Add the connected client data to the game instance
				gameInstance->AddPlayerData(ClientData.clientId, ClientData);
				// Load the GameLevel
				gameInstance->LoadLevel(gameInstance->GameLevelName);
				// Spawn the player for the current client
				gameInstance->SpawnPlayerForClient(ClientData.clientId);

				// Get connected players
				SendGetConnectedPlayersRequest(ClientData);
			}
			else {
				// ue log position
				UE_LOG(LogTemp, Warning, TEXT("Init Position of other Player: X: %f, Y: %f, Z: %f"), ClientData.characterData.characterPosition.positionX, ClientData.characterData.characterPosition.positionY, ClientData.characterData.characterPosition.positionZ);
				// Add the connected client data to the game instance
				gameInstance->AddPlayerData(ClientData.clientId, ClientData);
				gameInstance->SpawnPlayerForClient(ClientData.clientId);
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
			// get connected players deserialize
			TArray<FClientDataStruct> ConnectedPlayers = JSONParser::DeserializeCharactersList(Body);

			// go through connected players
			for (const FClientDataStruct& ConnectedPlayer : ConnectedPlayers) {
				// If the client ID is not the same as the current client ID
				if (gameInstance->GetCurrentClientID() != ConnectedPlayer.clientId) {
					//debug current client id and connected player client id 
					UE_LOG(LogTemp, Warning, TEXT("Connected Player Client ID: %d, Current Client ID: %d"), ConnectedPlayer.clientId, gameInstance->GetCurrentClientID());

					gameInstance->AddPlayerData(ConnectedPlayer.clientId, ConnectedPlayer);
					gameInstance->SpawnPlayerForClient(ConnectedPlayer.clientId);
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
		if (gameInstance && IsValid(gameInstance)) {
			// If the client ID is not the same as the current client ID
			if (gameInstance->GetCurrentClientID() != ClientData.clientId) {
				// ue log position
				UE_LOG(LogTemp, Warning, TEXT("MovePlayerPosition X: %f, Y: %f, Z: %f"), ClientData.characterData.characterPosition.positionX, ClientData.characterData.characterPosition.positionY, ClientData.characterData.characterPosition.positionZ);

				// Update the player coordinates
				gameInstance->MovePlayerForClient(ClientData.clientId, ClientData, MessageData);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerManager: GameInstance is null or invalid in moveCharacter"));
		}
	}

	// If the data is a response to a leave game request
	if (MessageData.eventType == "disconnectClient" && ClientData.clientId != 0) {
		if (gameInstance && IsValid(gameInstance)) {
			// If the client ID is not the same as the current client ID
			if (gameInstance->GetCurrentClientID() != ClientData.clientId) {
				gameInstance->HandlePlayerDisconnection(ClientData.clientId);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PlayerManager: GameInstance is null or invalid in disconnectClient"));
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
	BodyData.Add("characterId", CharacterIDValue);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("joinGame", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::GameServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the game server
		networkManager->SendDataToGameServer(JsonString);
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
	// Add the character ID to the body data
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

	BodyData.Add("id", CharacterIDValue);

	// Add position data to the body data
	TSharedPtr<FJsonValueNumber> PositionXValue = MakeShareable(new FJsonValueNumber(ClientData.characterData.characterPosition.positionX));
	TSharedPtr<FJsonValueNumber> PositionYValue = MakeShareable(new FJsonValueNumber(ClientData.characterData.characterPosition.positionY));
	TSharedPtr<FJsonValueNumber> PositionZValue = MakeShareable(new FJsonValueNumber(ClientData.characterData.characterPosition.positionZ));
	TSharedPtr<FJsonValueNumber> RotationZValue = MakeShareable(new FJsonValueNumber(ClientData.characterData.characterPosition.rotationZ));

	BodyData.Add("posX", PositionXValue);
	BodyData.Add("posY", PositionYValue);
	BodyData.Add("posZ", PositionZValue);
	BodyData.Add("rotZ", RotationZValue);

	// Debug logging to check TimeSyncService availability
	UTimeSyncService* TimeSyncService = gameInstance ? gameInstance->GetTimeSyncService() : nullptr;
	if (!TimeSyncService)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerManager::SendMovePlayerRequest - TimeSyncService is null! gameInstance: %s"),
			gameInstance ? TEXT("Valid") : TEXT("Null"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerManager::SendMovePlayerRequest - TimeSyncService is available"));
	}

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("moveCharacter", HeaderData, BodyData, TimeSyncService, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the chunk server
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

	BodyData.Add("id", CharacterIDValue);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("disconnectClient", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::LoginServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the game server
		networkManager->SendDataToGameServer(JsonString);
		networkManager->SendDataToChunkServer(JsonString);
		networkManager->SendDataToLoginServer(JsonString);
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

	FString JsonString = JSONParser::SerializeJsonWithTimeSync("respawn", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

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
