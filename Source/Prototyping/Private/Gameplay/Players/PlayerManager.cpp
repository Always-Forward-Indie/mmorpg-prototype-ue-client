// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/PlayerManager.h"
#include "MyGameInstance.h"

UPlayerManager::UPlayerManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPlayerManager::Initialize(UNetworkManager* NetworkManager, UPingManager* PingManager)
{
	networkManager = NetworkManager;
	pingManager = PingManager;
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

// Add receive ping time
void UPlayerManager::AddReceivePingTime(const FString& EventType, const FDateTime& ReceiveTime)
{
	// Add the received timestamp to the array, indexed by event type
	if (!ReceiveTimes.Contains(EventType))
	{
		ReceiveTimes.Add(EventType, TArray<FDateTime>());
	}
	ReceiveTimes[EventType].Add(ReceiveTime);

	// Check if there are sufficient send and receive timestamps for this event type
	if (SendTimes.Contains(EventType) && ReceiveTimes.Contains(EventType) &&
		SendTimes[EventType].Num() >= PingPacketsCount && ReceiveTimes[EventType].Num() >= PingPacketsCount &&
		pingManager)
	{
		// Calculate average ping time using the send and receive timestamps for this event type
		pingManager->CalculatePingTime(SendTimes[EventType], ReceiveTimes[EventType], "Game Server");

		// Clear the arrays to start collecting new values (assuming one-to-one correspondence)
		SendTimes[EventType].Empty();
		ReceiveTimes[EventType].Empty();
	}
}

// Add send ping time
void UPlayerManager::AddSendPingTime(const FString& EventType, const FDateTime& SendTime)
{
	// Add the send timestamp to the array, indexed by event type
	if (!SendTimes.Contains(EventType))
	{
		SendTimes.Add(EventType, TArray<FDateTime>());
	}
	SendTimes[EventType].Add(SendTime);
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

			//TODO - add connection delegate call for connect to chunk sever

			SendJoinClientChunkRequest(ClientData);
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


	// If the ping manager is valid, calculate the ping time
	if (pingManager != nullptr && MessageData.eventType == "pingClient") {
		// Process the received data here
		AddReceivePingTime(MessageData.eventType, FDateTime::UtcNow());
	}
}

// Process chunk server data
void UPlayerManager::ProcessChunkServerData(const FString& ReceivedData)
{
	UE_LOG(LogTemp, Warning, TEXT("Init by delegate Chunk Server: %s"), *ReceivedData);


	// Разбор строки JSON один раз
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return;

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));

	// Deserialize the received JSON string to get MessageData struct
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	// Deserialize the received JSON string to get ClientData struct
	FClientDataStruct ClientData = JSONParser::DeserializeClientData(ReceivedData);
	// Deserialize the received JSON string to get CharacterData struct
	FCharacterDataStruct CharacterData = JSONParser::DeserializeCharacterData(Body->GetObjectField(TEXT("character")));

	//if character data is valid
	if (CharacterData.characterId != 0) {
		// Set the character data
		ClientData.characterData = CharacterData;
	}


	// If the data is a response to a join game request
	if (MessageData.eventType == "joinGameCharacter" && MessageData.status == "success" && ClientData.clientId != 0) {
		if (gameInstance) {
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

				// Get connected players
				//SendGetConnectedPlayersRequest(ClientData);
			}

		}
	}

	// If the data is a response to a get connected players request
	if (MessageData.eventType == "getConnectedCharacters" && MessageData.status == "success" && ClientData.clientId != 0) {
		if (gameInstance) {
			// get connected players deserialize
			TArray<FClientDataStruct> ConnectedPlayers = JSONParser::DeserializeCharactersList(Root->GetObjectField(TEXT("body")));

			// go through connected players
			for (FClientDataStruct ConnectedPlayer : ConnectedPlayers) {
				// If the client ID is not the same as the current client ID
				if (gameInstance->GetCurrentClientID() != ConnectedPlayer.clientId) {
					//debug current client id and connected player client id 
					UE_LOG(LogTemp, Warning, TEXT("Connected Player Client ID: %d, Current Client ID: %d"), ConnectedPlayer.clientId, gameInstance->GetCurrentClientID());


					gameInstance->AddPlayerData(ConnectedPlayer.clientId, ConnectedPlayer);
					gameInstance->SpawnPlayerForClient(ConnectedPlayer.clientId);
				}
			}
		}
	}


	// If the data is a response to a move player request
	if (MessageData.eventType == "moveCharacter" && MessageData.status == "success" && CharacterData.characterId != 0) {
		if (gameInstance) {
			// If the client ID is not the same as the current client ID
			if (gameInstance->GetCurrentClientID() != ClientData.clientId) {
				// ue log position
				UE_LOG(LogTemp, Warning, TEXT("MovePlayerPosition X: %f, Y: %f, Z: %f"), ClientData.characterData.characterPosition.positionX, ClientData.characterData.characterPosition.positionY, ClientData.characterData.characterPosition.positionZ);

				// Update the player coordinates
				gameInstance->MovePlayerForClient(ClientData.clientId, ClientData, MessageData);
			}
		}
	}

	// Обработка анимации боя
	if (MessageData.eventType == "combatAnimation" && MessageData.status == "success") {
		FCombatAnimationData AnimationData = JSONParser::DeserializeCombatAnimation(Body->GetObjectField("animation"));

		UE_LOG(LogTemp, Warning, TEXT("Combat Animation: %s for Character %d"),
			*AnimationData.AnimationName, AnimationData.CharacterId);

		// Здесь можно добавить воспроизведение анимации
		if (gameInstance) {
			gameInstance->PlayCombatAnimation(AnimationData);
		}
	}

	// Обработка инициации боевого действия
	if (MessageData.eventType == "initiateCombatAction" && MessageData.status == "success") {
		FCombatActionData ActionData = JSONParser::DeserializeCombatAction(Body->GetObjectField("action"));

		UE_LOG(LogTemp, Warning, TEXT("Combat Action: %s from Caster %d to Target %d (%s)"),
			*ActionData.ActionName, ActionData.CasterId, ActionData.TargetId,
			*ActionData.TargetTypeString);

		// Можно добавить визуальные эффекты атаки
		if (gameInstance) {
			gameInstance->ProcessCombatAction(ActionData);
		}
	}

	// Обработка результата боя - УНИВЕРСАЛЬНОЕ ОБНОВЛЕНИЕ ЗДОРОВЬЯ
	if (MessageData.eventType == "combatResult" && MessageData.status == "success") {
		FCombatResultData ResultData = JSONParser::DeserializeCombatResult(Body->GetObjectField("result"));

		UE_LOG(LogTemp, Warning, TEXT("Combat Result: Target %d (%s) took %d damage, Health: %d, Mana: %d, Dead: %s"),
			ResultData.TargetId, *ResultData.TargetTypeString, ResultData.DamageDealt,
			ResultData.RemainingHealth, ResultData.RemainingMana,
			ResultData.bTargetDied ? TEXT("Yes") : TEXT("No"));

			// ИСПОЛЬЗУЕМ УНИВЕРСАЛЬНУЮ ФУНКЦИЮ UpdateTargetHealth
		if (gameInstance) {
			gameInstance->UpdateTargetHealth(
				ResultData.TargetId, 
				ResultData.TargetType, 
				ResultData.TargetTypeString,
				ResultData.RemainingHealth,
				ResultData.RemainingMana, 
				ResultData.bTargetDied,
				ResultData.bIsDamaged,
				ResultData.DamageDealt
			);
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


	// If the ping manager is valid, calculate the ping time
	if (pingManager != nullptr && MessageData.eventType == "pingClient") {
		// Process the received data here
		AddReceivePingTime(MessageData.eventType, FDateTime::UtcNow());
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

	FString JsonString = JSONParser::SerializeJson("joinGame", HeaderData, BodyData);

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

	FString JsonString = JSONParser::SerializeJson("joinGameCharacter", HeaderData, BodyData);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the Chunk server
		networkManager->SendDataToChunkServer(JsonString);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
}

void UPlayerManager::SendJoinClientChunkRequest(const FClientDataStruct& ClientData)
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

	FString JsonString = JSONParser::SerializeJson("joinGameClient", HeaderData, BodyData);

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

	// Get Connected Characters
	FString getConnectedCharacters = JSONParser::SerializeJson("getConnectedCharacters", HeaderData, BodyData);

	// Get spawn zones and spawn mobs
	FString getSpawnZones = JSONParser::SerializeJson("getSpawnZones", HeaderData, BodyData);

	// Send the JSON string to the chunk server
	if (networkManager != nullptr)
	{
		// Send the JSON string to the chunk server
		networkManager->SendDataToChunkServer(getConnectedCharacters);
		networkManager->SendDataToChunkServer(getSpawnZones);
	}
	else {
		UE_LOG(LogTemp, Error, TEXT("Network manager not found"));
	}
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

	FString JsonString = JSONParser::SerializeJson("moveCharacter", HeaderData, BodyData);

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

	FString JsonString = JSONParser::SerializeJson("disconnectClient", HeaderData, BodyData);

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

//send ping request
void UPlayerManager::SendPingRequest()
{
	// Create the header JSON object
	TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
	HeaderObject->SetStringField("eventType", "pingClient");

	// Create the main JSON object and add header and body
	TSharedPtr<FJsonObject> MainJsonObject = MakeShareable(new FJsonObject);
	MainJsonObject->SetObjectField("header", HeaderObject);

	// Serialize the JSON object to a string
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);

	// Удалим все символы перевода строки
	OutputString.ReplaceInline(TEXT("\n"), TEXT(""));
	OutputString.ReplaceInline(TEXT("\r"), TEXT(""));


	if (networkManager != nullptr)
	{
		// add send time
		AddSendPingTime("pingClient", FDateTime::UtcNow());

		networkManager->SendDataToGameServer(OutputString);
		networkManager->SendDataToChunkServer(OutputString);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Network Manager not found"));
	}
}

void UPlayerManager::StartPing()
{
	if (worldContext)
	{
		worldContext->GetTimerManager().ClearTimer(NetworkServersPingTimerHandle);
		worldContext->GetTimerManager().SetTimer(NetworkServersPingTimerHandle, this, &UPlayerManager::SendPingRequest, PingTimeout, true);
		UE_LOG(LogTemp, Warning, TEXT("Ping timer for Game server set up successfully."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GetWorld() returned nullptr, ping timer for servers not set."));
	}
}

void UPlayerManager::SendPlayerAttackRequest(const FClientDataStruct& ClientData, int32 TargetID, int32 ActionID, bool bUseAI, const FString& TargetType)
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
	TSharedPtr<FJsonValueNumber> ActionIDValue = MakeShareable(new FJsonValueNumber(ActionID));
	TSharedPtr<FJsonValueNumber> TargetIDValue = MakeShareable(new FJsonValueNumber(TargetID));
	TSharedPtr<FJsonValueBoolean> UseAIValue = MakeShareable(new FJsonValueBoolean(bUseAI));
	TSharedPtr<FJsonValueString> TargetTypeValue = MakeShareable(new FJsonValueString(TargetType));

	BodyData.Add("actionId", ActionIDValue);
	BodyData.Add("targetId", TargetIDValue);
	BodyData.Add("useAI", UseAIValue);
	BodyData.Add("targetType", TargetTypeValue);

	// Serialize to JSON string with the correct event type
	FString JsonString = JSONParser::SerializeJson("PLAYER_ATTACK", HeaderData, BodyData);

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
