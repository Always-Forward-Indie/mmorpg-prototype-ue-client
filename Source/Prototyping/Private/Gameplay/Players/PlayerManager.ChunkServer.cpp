// Fill out your copyright notice in the Description page of Project Settings.



#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "MyGameInstance.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"

void UPlayerManager::HandleChunkServerSocketConnected()
{
	if (!gameInstance || !IsValid(gameInstance))
	{
		return;
	}
	// Initial connects happen before the game world is live — the normal
	// game-server flow (Phase PRE) drives the join. Only a reconnect lands
	// here with a live world: resend Phase 1 so the server rebinds the new
	// socket to our clientId (it drops all state on disconnect). Phase 2/3
	// then continue through the regular response handlers below.
	if (!gameInstance->IsGameWorldReady())
	{
		return;
	}
	const int32 OwnClientId = gameInstance->GetCurrentClientID();
	if (OwnClientId <= 0)
	{
		return;
	}
	FClientDataStruct LocalClientData = gameInstance->GetCurrentClientData();
	UE_LOG(LogConnection, Warning, TEXT("[REJOIN] chunk reconnected mid-session, clientId=%d charId=%d — resending Phase 1 joinGameClient"),
		OwnClientId, LocalClientData.characterData.characterId);
	SendJoinClientChunkRequest(LocalClientData);
}

void UPlayerManager::ProcessChunkServerData(const FString& ReceivedData)
{
	// Legacy FString entry kept for compat; the live path is ProcessChunkPacket
	// via the dispatcher fallback (single parse in PollLink).
	ProcessChunkPacket(JSONParser::ParsePacket(ReceivedData));
}

void UPlayerManager::ProcessChunkPacket(const FParsedPacket& Packet)
{
	CRASH_GUARD("PlayerManager::ProcessChunkPacket");

	// Compat alias: a few branches below still take the raw string (S05b converts them).
	const FString& ReceivedData = Packet.Raw;

	// Single-parse path (S05): Root/Body/EventType come from PollLink.
	if (!Packet.bValid || !Packet.Root.IsValid())
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: Received empty data"));
		return;
	}

	TSharedPtr<FJsonObject> Body;
	if (Packet.Root->HasField(TEXT("body")))
	{
		Body = Packet.Root->GetObjectField(TEXT("body"));
		if (!Body.IsValid())
		{
			UE_LOG(LogConnection, Error, TEXT("PlayerManager: Invalid body object"));
			return;
		}
	}

	// Deserialize the packet to get MessageData struct (no re-parse: works on Root)
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(Packet);

	// Проверяем валидность MessageData
	if (MessageData.eventType.IsEmpty())
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: Empty event type"));
		return;
	}

	// Deserialize client data from the parsed root (no re-parse)
	FClientDataStruct ClientData = JSONParser::DeserializeClientData(Packet.Root);
	
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
			// Only react to the response that belongs to this GameInstance's own client session
			if (ClientData.clientId != gameInstance->GetCurrentClientID())
			{
				UE_LOG(LogConnection, Warning, TEXT("[PHASE 1] joinGameClient OK for clientId=%d but our clientId=%d — ignoring"),
					ClientData.clientId, gameInstance->GetCurrentClientID());
			}
			else
			{
				// Restore characterId from GameInstance — the ChunkServer joinGameClient response
				// does not echo the characterId back, so ClientData.characterData.characterId is 0 here.
				FClientDataStruct LocalClientData = ClientData;
				LocalClientData.characterData.characterId = gameInstance->GetCurrentCharacterID();
				UE_LOG(LogConnection, Log, TEXT("[PHASE 1] joinGameClient OK on ChunkServer, clientId=%d charId=%d -> sending joinGameCharacter"),
					LocalClientData.clientId, LocalClientData.characterData.characterId);
				// ChunkServer is now game-ready: it has registered our socket and knows our clientId.
				// Unlock ChunkServer pings so TimeSyncService can start measuring RTT to ChunkServer.
				if (UPingManager* PM = gameInstance->GetPingManager())
				{
					PM->SetChunkServerGameReady(true);
				}
				SendJoinCharacterChunkRequest(LocalClientData);
			}
		}
	}

	// Phase 3: playerReady ACK from server
	if (MessageData.eventType == "playerReady" && MessageData.status == "success") {
		if (gameInstance && IsValid(gameInstance)) {
			UE_LOG(LogConnection, Log, TEXT("[PHASE 3] playerReady ACK received - Phase 4 world-state incoming"));
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
					UE_LOG(LogConnection, Log, TEXT("positionCorrection: clientId=%d (%.1f, %.1f, %.1f) rot=%.2f"),
						ClientData.clientId, CorrX, CorrY, CorrZ, CorrRotZ);
					gameInstance->UpdatePlayerCoordinates(ClientData.clientId, CorrX, CorrY, CorrZ, CorrRotZ);
				}
			}
		}
	}

	// Phase 1 continued: joinGameCharacter response
	if (MessageData.eventType == "joinGameCharacter" && MessageData.status == "success" && ClientData.clientId != 0) {
		if (gameInstance && IsValid(gameInstance)) {
			UE_LOG(LogConnection, Log, TEXT("[PHASE 2] joinGameCharacter OK: clientId=%d charId=%d pos=(%.1f,%.1f,%.1f)"),
				ClientData.clientId, ClientData.characterData.characterId,
				ClientData.characterData.characterPosition.positionX,
				ClientData.characterData.characterPosition.positionY,
				ClientData.characterData.characterPosition.positionZ);

			// If the client ID is the same as the current client ID
			if (gameInstance->GetCurrentClientID() == ClientData.clientId) {
				// Store player data before world transition
				gameInstance->AddPlayerData(ClientData.clientId, ClientData);
				// Rejoin after a mid-session reconnect: the world is already
				// live and our actor spawned — travelling again would flash
				// the loading screen and reset readiness for nothing. Skip
				// the travel, restore the characterId the response omits,
				// and resend playerReady so the server pushes a fresh
				// Phase-4 world-state snapshot over the new socket.
				const int32 OwnCharId = gameInstance->GetCurrentCharacterID();
				if (gameInstance->IsGameWorldReady() && gameInstance->GetPlayerByCharacterId(OwnCharId) != nullptr)
				{
					FClientDataStruct RejoinData = ClientData;
					RejoinData.characterData.characterId = OwnCharId;
					UE_LOG(LogConnection, Warning, TEXT("[PHASE 2] rejoin OK: clientId=%d charId=%d — world live, skipping travel, resending playerReady"),
						RejoinData.clientId, OwnCharId);
					SendPlayerReadyRequest(RejoinData);
				}
				else
				{
					// Queue local player spawn for after the game world loads
					gameInstance->PendingSpawnClientId = ClientData.clientId;
					// Transition to game world (OpenLevel for World Partition)
					gameInstance->TransitionToGameWorld();
				}
			}
			else {
				// Remote player: only queue/spawn if we have valid position data
				const bool bHasPosition = (ClientData.characterData.characterId != 0);
				if (!bHasPosition)
				{
					UE_LOG(LogConnection, Warning, TEXT("[PHASE 2] joinGameCharacter: skipping remote spawn for clientId=%d — no character data"), ClientData.clientId);
				}
				else if (gameInstance->IsGameWorldReady())
				{
					UE_LOG(LogConnection, Warning, TEXT("[PHASE 2] joinGameCharacter REMOTE: clientId=%d charId=%d — world ready, spawning"),
						ClientData.clientId, ClientData.characterData.characterId);
					gameInstance->AddPlayerData(ClientData.clientId, ClientData);
					gameInstance->SpawnPlayerForClient(ClientData.clientId);
				}
				else
				{
					gameInstance->PendingRemotePlayerSpawns.Add(ClientData);
					UE_LOG(LogConnection, Warning, TEXT("[PHASE 2] joinGameCharacter REMOTE: clientId=%d charId=%d — world NOT ready, queued"),
						ClientData.clientId, ClientData.characterData.characterId);
				}
			}
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("PlayerManager: GameInstance is null or invalid in joinGameCharacter"));
		}
	}

	// If the data is a response to a get connected players request
	if ((MessageData.eventType == "getConnectedCharacters" || MessageData.eventType == "getConnectedClients") && MessageData.status == "success" && ClientData.clientId != 0) {
		if (gameInstance && IsValid(gameInstance)) {
			const TArray<TSharedPtr<FJsonValue>>* ClientsListEntries = nullptr;
		const bool bHasClientsList = Body.IsValid() && Body->TryGetArrayField(TEXT("clientsList"), ClientsListEntries) && ClientsListEntries != nullptr;
		if (bHasClientsList)
		{
			// New lightweight format (getConnectedClients): body.clientsList[] = {clientId, characterId, status}.
			// It carries no character snapshots, so entries are only logged here; full spawns arrive via joinGameCharacter.
			for (const TSharedPtr<FJsonValue>& ClientValue : *ClientsListEntries)
			{
				const TSharedPtr<FJsonObject> ClientObj = ClientValue.IsValid() ? ClientValue->AsObject() : nullptr;
				if (!ClientObj.IsValid()) continue;
				int32 ListedClientId = 0;
				int32 ListedCharacterId = 0;
				FString ListedStatus;
				ClientObj->TryGetNumberField(TEXT("clientId"), ListedClientId);
				ClientObj->TryGetNumberField(TEXT("characterId"), ListedCharacterId);
				ClientObj->TryGetStringField(TEXT("status"), ListedStatus);
				if (ListedClientId == 0 || gameInstance->GetCurrentClientID() == ListedClientId) continue;
				UE_LOG(LogConnection, Log, TEXT("[getConnectedClients] online clientId=%d charId=%d status=%s"), ListedClientId, ListedCharacterId, *ListedStatus);
			}
		}
		else
		{
		TArray<FClientDataStruct> ConnectedPlayers = JSONParser::DeserializeCharactersList(Body);

			for (const FClientDataStruct& ConnectedPlayer : ConnectedPlayers) {
				if (gameInstance->GetCurrentClientID() != ConnectedPlayer.clientId) {
					UE_LOG(LogConnection, Log, TEXT("[getConnectedCharacters] remote clientId=%d charId=%d pos=(%.1f,%.1f,%.1f)"),
						ConnectedPlayer.clientId, ConnectedPlayer.characterData.characterId,
						ConnectedPlayer.characterData.characterPosition.positionX,
						ConnectedPlayer.characterData.characterPosition.positionY,
						ConnectedPlayer.characterData.characterPosition.positionZ);

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
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("PlayerManager: GameInstance is null or invalid in getConnectedCharacters"));
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
			UE_LOG(LogConnection, Error, TEXT("PlayerManager: GameInstance is null or invalid in disconnectClient"));
		}
	}

	// Handle experience_update (XP gain, level up)
	if (MessageData.eventType == "experience_update" && MessageData.status == "success")
	{
		FExperienceUpdateStruct ExpUpdate = JSONParser::DeserializeExperienceUpdate(ReceivedData);

		if (gameInstance && IsValid(gameInstance) && ExpUpdate.characterId > 0)
		{
			UExperienceManager* ExpMgr = gameInstance->GetExperienceManager();
			if (ExpMgr && IsValid(ExpMgr))
			{
				ExpMgr->ProcessExperienceUpdate(ExpUpdate);
				UE_LOG(LogConnection, Log, TEXT("experience_update: charId=%d +%d XP Level %d->%d LevelUp=%s"),
					ExpUpdate.characterId, ExpUpdate.experienceChange,
					ExpUpdate.oldLevel, ExpUpdate.newLevel,
					ExpUpdate.levelUp ? TEXT("true") : TEXT("false"));
			}
			else
			{
				UE_LOG(LogConnection, Warning, TEXT("PlayerManager: ExperienceManager not available for experience_update"));
			}
		}
	}

	// Handle player stats update
	if (MessageData.eventType == "stats_update" && MessageData.status == "success")
	{
		FPlayerStatsUpdateStruct StatsUpdate = JSONParser::DeserializePlayerStatsUpdate(ReceivedData);

		if (gameInstance && IsValid(gameInstance))
		{
			const int32 CharacterId = StatsUpdate.characterId;
			if (CharacterId > 0)
			{
				ABasicPlayer* Player = gameInstance->GetPlayerByCharacterId(CharacterId);
				if (Player && IsValid(Player))
				{
					Player->ProcessStatsUpdate(StatsUpdate);
					UE_LOG(LogConnection, Log, TEXT("stats_update: charId=%d applied"), CharacterId);
				}
				else
				{
					// Player actor not yet registered (early login packet).
					// Only push to PlayerStatsManager for OUR OWN character — if the server
					// broadcasts another player's stats_update and that actor isn't spawned
					// yet, we must not let foreign stats overwrite the local player's HUD.
					if (CharacterId == gameInstance->GetCurrentCharacterID())
					{
						UE_LOG(LogConnection, Warning, TEXT("stats_update: charId=%d actor missing — pushing to PlayerStatsManager (own char)"), CharacterId);
						if (UPlayerStatsManager* StatsMgr = gameInstance->GetPlayerStatsManager())
						{
							StatsMgr->ApplyStatsUpdate(StatsUpdate);
						}
					}
					else
					{
						UE_LOG(LogConnection, Warning, TEXT("stats_update: charId=%d actor missing and is NOT local char (%d) — discarding"),
							CharacterId, gameInstance->GetCurrentCharacterID());
					}
				}
			}
			else
			{
				UE_LOG(LogConnection, Error, TEXT("stats_update: invalid charId=%d"), CharacterId);
			}
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("PlayerManager: GameInstance is null in stats_update"));
		}
	}

	// Handle respawnResult - server response to respawnRequest
	if (MessageData.eventType == "respawnResult")
	{
		if (MessageData.status != "success")
		{
			UE_LOG(LogConnection, Error, TEXT("respawnResult failed: %s"), *MessageData.message);

			int32 RespawnCharId = 0;
			if (Body.IsValid()) Body->TryGetNumberField(TEXT("characterId"), RespawnCharId);
			if (RespawnCharId <= 0 && ClientData.clientId > 0)
			{
				if (const FClientDataStruct* Stored = gameInstance->ConnectedPlayers.Find(ClientData.clientId))
					RespawnCharId = Stored->characterData.characterId;
			}
			ABasicPlayer* RespawnPlayer = gameInstance->GetPlayerByCharacterId(RespawnCharId);
			if (RespawnPlayer && IsValid(RespawnPlayer))
			{
				// Server says "already alive" — client-server desync. Force-revive to reconcile.
				if (MessageData.message.Contains(TEXT("not dead")) || MessageData.message.Contains(TEXT("already alive")) || MessageData.message.Contains(TEXT("not_dead")) || MessageData.message.Contains(TEXT("while alive")))
				{
					UE_LOG(LogConnection, Warning, TEXT("respawnResult: server says player is already alive — force-reviving client"));
					RespawnPlayer->SetDead_Implementation(false);
				}
			}
			return;
		}

		if (gameInstance && IsValid(gameInstance) && Body.IsValid())
		{
			int32 RespawnCharId = 0;
			Body->TryGetNumberField(TEXT("characterId"), RespawnCharId);

			if (RespawnCharId <= 0 && ClientData.clientId > 0)
			{
				// Server omitted characterId — resolve it from the connected player map
				// using the clientId that was deserialized from this packet.
				if (const FClientDataStruct* Stored = gameInstance->ConnectedPlayers.Find(ClientData.clientId))
				{
					RespawnCharId = Stored->characterData.characterId;
				}
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
					RespawnPlayer->SetCoordinates(RX, RY, RZ, RRot, false);
				}
				else
				{
					UE_LOG(LogConnection, Warning, TEXT("respawnResult: position field missing or null for charId=%d — reviving in-place"), RespawnCharId);
				}

				// Revive the player: restores movement mode, hides death screen,
				// plays revive sound, notifies AnimBP. Must happen AFTER teleport so
				// CMC re-enables walking at the correct world position.
				RespawnPlayer->SetDead_Implementation(false);
				UE_LOG(LogConnection, Log, TEXT("respawnResult: charId=%d respawned"), RespawnCharId);
			}
			else
			{
				UE_LOG(LogConnection, Error, TEXT("respawnResult: player not found for charId=%d"), RespawnCharId);
			}
		}
	}
}

