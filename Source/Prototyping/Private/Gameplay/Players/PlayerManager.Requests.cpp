// Fill out your copyright notice in the Description page of Project Settings.



#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "MyGameInstance.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"

void UPlayerManager::StartPing()
{
	if (worldContext && pingManager)
	{
		// Set world context for PingManager
		pingManager->SetWorldContext(worldContext);
		
		// Start the new TimeSyncService-based ping updates
		pingManager->StartPingUpdates();
		UE_LOG(LogPing, Log, TEXT("PlayerManager: Started ping updates"));
	}
	else
	{
		UE_LOG(LogConnection, Warning, TEXT("PlayerManager: Cannot start ping - missing worldContext or pingManager"));
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

	// Add the Character ID to the body data (required per protocol §1.0) — use ClientData exclusively
	const int32 CharId = ClientData.characterData.characterId;
	BodyData.Add("characterId", MakeShareable(new FJsonValueNumber(CharId)));

	// Protocol framing (S14): declare v2 support (transport mode itself is
	// sniffed by the server from the first bytes).
	JSONParser::AddFramingOptIn(BodyData);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("joinGameClient", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::GameServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the game server
		networkManager->SendDataToGameServer(JsonString);
		UE_LOG(LogConnection, Log, TEXT("[PHASE PRE] joinGameClient -> GameServer, charId=%d"), CharId);
	}
	else {
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendJoinGameRequest"));
	}
}

void UPlayerManager::SendJoinClientChunkRequest(const FClientDataStruct& ClientData)
{
	// Phase 1: Register client session on Chunk Server
	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	// Transport capability (S13-C1): opt in from the FIRST message so the
	// server knows before any fat response is queued.
	JSONParser::AddFragOptIn(BodyData);
	// Protocol framing (S14): declare v2 support (transport mode itself is
	// sniffed by the server from the first bytes).
	JSONParser::AddFramingOptIn(BodyData);

	// Add the client ID and hash to the header data per protocol
	HeaderData.Add("clientId", MakeShareable(new FJsonValueNumber(ClientData.clientId)));
	HeaderData.Add("hash", MakeShareable(new FJsonValueString(ClientData.hash)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync("joinGameClient", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		networkManager->SendDataToChunkServer(JsonString);
		UE_LOG(LogConnection, Log, TEXT("[PHASE 1] joinGameClient -> ChunkServer"));
	}
	else {
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendJoinClientChunkRequest"));
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

	// Per protocol: body has characterId — use ClientData exclusively
	const int32 CharId = ClientData.characterData.characterId;
	BodyData.Add("characterId", MakeShareable(new FJsonValueNumber(CharId)));
	// Transport fragmentation (wireFrag): the server fragments oversize
	// lines for sockets that opt in here (single helper for all join
	// messages — the flag is learned from the first one carrying it).
	JSONParser::AddFragOptIn(BodyData);
	// Protocol framing (S14): declare v2 support.
	JSONParser::AddFramingOptIn(BodyData);

	FString JsonString = JSONParser::SerializeJsonWithTimeSync("playerReady", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		networkManager->SendDataToChunkServer(JsonString);
		UE_LOG(LogConnection, Log, TEXT("[PHASE 3] playerReady -> ChunkServer, charId=%d"), CharId);
	}
	else {
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendPlayerReadyRequest"));
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

	// Add the Character ID to the body data — use ClientData exclusively.
	// The caller (ProcessChunkServerData) is responsible for populating characterId
	// from GameInstance before invoking this function.
	const int32 CharId = ClientData.characterData.characterId;
	BodyData.Add("id", MakeShareable(new FJsonValueNumber(CharId)));

	// S2.1: report our cached item stat catalog version (0 = unknown).
	// The server pushes the full itemCatalog only on mismatch.
	int32 CatalogVersion = 0;
	if (gameInstance)
	{
		if (UItemManager* ItemMgr = gameInstance->GetItemManager())
			CatalogVersion = ItemMgr->GetCatalogVersion();
	}
	BodyData.Add("itemCatalogVersion", MakeShareable(new FJsonValueNumber(CatalogVersion)));
	// Transport capability (S13-C1): duplicate the opt-in here too — every
	// chunk-bound join message carries it, the server learns from the first.
	JSONParser::AddFragOptIn(BodyData);
	// Protocol framing (S14): declare v2 support.
	JSONParser::AddFramingOptIn(BodyData);

	// Use TimeSyncService for automatic clientSendMs with correct server type
	FString JsonString = JSONParser::SerializeJsonWithTimeSync("joinGameCharacter", HeaderData, BodyData, gameInstance ? gameInstance->GetTimeSyncService() : nullptr, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		// Send the JSON string to the Chunk server
		networkManager->SendDataToChunkServer(JsonString);
		UE_LOG(LogConnection, Log, TEXT("[PHASE 2] joinGameCharacter -> ChunkServer, clientId=%d charId=%d"),
			ClientData.clientId, CharId);
	}
	else {
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendJoinCharacterChunkRequest"));
	}
}

void UPlayerManager::SendGetConnectedPlayersRequest(FClientDataStruct& ClientData)
{
	if (!networkManager || !IsValid(networkManager))
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendGetConnectedPlayersRequest"));
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

	// NOTE: getSpawnZones is intentionally NOT sent — the chunk server does not
	// implement it ("Unknown event type", session breaks afterwards) and the
	// server auto-pushes Phase 4 world-state after playerReady ACK instead.

	// Validate JSON strings before sending
	if (getConnectedCharacters.IsEmpty())
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: Failed to serialize JSON data in SendGetConnectedPlayersRequest"));
		return;
	}

	// Send the JSON string to the chunk server
	networkManager->SendDataToChunkServer(getConnectedCharacters);
}

void UPlayerManager::SendMovePlayerRequest(FClientDataStruct& ClientData)
{
	// DevMode offline: movement is local-only, no chunk to report to.
	// Skip quietly (Verbose) instead of Error-spamming every tick.
	// Live path (DevMode off) is unchanged.
	if (gameInstance && gameInstance->DevModeConfig.bEnabled)
	{
		UE_LOG(LogConnection, Verbose, TEXT("PlayerManager: skipping SendMovePlayerRequest (DevMode offline)"));
		return;
	}

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
	BodyData.Add("isFalling", MakeShareable(new FJsonValueBoolean(ClientData.characterData.bIsFalling)));

	UTimeSyncService* TimeSyncService = gameInstance ? gameInstance->GetTimeSyncService() : nullptr;

	FString JsonString = JSONParser::SerializeJsonWithTimeSync("moveCharacter", HeaderData, BodyData, TimeSyncService, EServerType::ChunkServer);

	if (networkManager != nullptr)
	{
		networkManager->SendDataToChunkServer(JsonString);
	}
	else {
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendMovePlayerRequest"));
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
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendLeaveGameRequest"));
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
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendPlayerAttackRequest"));
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
		UE_LOG(LogConnection, Log, TEXT("respawnRequest sent, charId=%d"), ClientData.characterData.characterId);
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null in SendRespawnRequest"));
	}
}
