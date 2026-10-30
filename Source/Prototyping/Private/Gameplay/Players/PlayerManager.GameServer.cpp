// Fill out your copyright notice in the Description page of Project Settings.



#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "MyGameInstance.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"

void UPlayerManager::ProcessGamePacket(const FParsedPacket& Packet)
{
	CRASH_GUARD("PlayerManager::ProcessGamePacket");

	// Single-parse path (S05): MessageData/ClientData come from PollLink, no re-parse.
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(Packet);
	// Deserialize the received JSON string to get ClientData struct
	FClientDataStruct ClientData = JSONParser::DeserializeClientData(Packet.Root);


	// If the data is a response to a join game request on Game Server
	if (MessageData.eventType == "joinGameClient" && MessageData.status == "success" && ClientData.clientId != 0 && ClientData.hash != "") {
		if (gameInstance) {
			if (ClientData.clientId != gameInstance->GetCurrentClientID())
			{
				UE_LOG(LogConnection, Warning, TEXT("[PHASE PRE] joinGameClient OK for clientId=%d but our clientId=%d — ignoring"),
					ClientData.clientId, gameInstance->GetCurrentClientID());
			}
			else
			{
				UE_LOG(LogConnection, Log, TEXT("[PHASE PRE] joinGameClient OK on GameServer, clientId=%d -> joining ChunkServer"), ClientData.clientId);
				SendJoinClientChunkRequest(ClientData);
			}
		}
	}

	// Join rejected by the Game Server: release the guard so the player can
	// retry, and surface the reason on the login UI (previously hung forever).
	if (MessageData.eventType == "joinGameClient" && MessageData.status != "success" && ClientData.clientId != 0) {
		if (gameInstance && IsValid(gameInstance) && gameInstance->IsJoinInProgress()
			&& ClientData.clientId == gameInstance->GetCurrentClientID()) {
			const FString Reason = !MessageData.errorCode.IsEmpty() ? MessageData.errorCode
				: (!MessageData.message.IsEmpty() ? MessageData.message : TEXT("join_rejected"));
			UE_LOG(LogConnection, Warning, TEXT("[PHASE PRE] joinGameClient FAILED on GameServer: %s"), *Reason);
			gameInstance->NotifyJoinFailed(Reason);
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
