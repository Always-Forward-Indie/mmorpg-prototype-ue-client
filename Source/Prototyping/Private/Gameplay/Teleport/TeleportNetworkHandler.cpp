#include "Gameplay/Teleport/TeleportNetworkHandler.h"
#include "Prototyping.h"
#include "Gameplay/Teleport/TeleportManager.h"
#include "Gameplay/Teleport/TeleportParser.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"

void UTeleportNetworkHandler::Initialize(UTeleportManager* InTeleportManager, UNetworkManager* InNetworkManager)
{
	if (!InTeleportManager || !InNetworkManager)
	{
		UE_LOG(LogConnection, Error, TEXT("TeleportNetworkHandler: Initialize called with null parameters"));
		return;
	}
	TeleportManager = InTeleportManager;
	NetworkManager = InNetworkManager;
}

void UTeleportNetworkHandler::SubscribeToNetworkEvents()
{
	if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
	FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
	for (const FString& EventType : HandledEvents())
	{
		Dispatcher.AddHandler(EventType, this, &UTeleportNetworkHandler::HandleChunkPacket);
	}
	bIsSubscribed = true;
}

void UTeleportNetworkHandler::UnsubscribeFromNetworkEvents()
{
	if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
	FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
	for (const FString& EventType : HandledEvents())
	{
		Dispatcher.RemoveHandler(EventType, this);
	}
	bIsSubscribed = false;
}

void UTeleportNetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
	if (!TeleportManager) return;

	const FString& EventType = Packet.EventType;
	if (!HandledEvents().Contains(EventType)) return;
	if (!Packet.Root.IsValid() || !Packet.Body.IsValid()) return;

	if (EventType == TEXT("respawnResult"))
	{
		HandleRespawnResult(Packet);
	}
	else if (EventType == TEXT("playerTeleported"))
	{
		HandlePlayerTeleported(Packet);
	}
}

const TArray<FString>& UTeleportNetworkHandler::HandledEvents()
{
	static const TArray<FString> Events = {
		TEXT("respawnResult"),
		TEXT("playerTeleported")
	};
	return Events;
}

void UTeleportNetworkHandler::HandleRespawnResult(const FParsedPacket& Packet)
{
	const TSharedPtr<FJsonObject>& Root = Packet.Root;
	const TSharedPtr<FJsonObject>& Body = Packet.Body;

	FClientDataStruct ClientData = JSONParser::DeserializeClientData(Root);

	if (Packet.Status != TEXT("success"))
	{
		UE_LOG(LogConnection, Error, TEXT("respawnResult failed: %s"), *Packet.Message);

		int32 RespawnCharId = 0;
		if (Body.IsValid()) Body->TryGetNumberField(TEXT("characterId"), RespawnCharId);
		RespawnCharId = TeleportManager->ResolveCharacterId(RespawnCharId, ClientData.clientId);

		ABasicPlayer* RespawnPlayer = TeleportManager->FindPlayerByCharacterId(RespawnCharId);
		if (RespawnPlayer && IsValid(RespawnPlayer))
		{
			// Server says "already alive" — client-server desync. Force-revive to reconcile.
			if (Packet.Message.Contains(TEXT("not dead")) || Packet.Message.Contains(TEXT("already alive"))
				|| Packet.Message.Contains(TEXT("not_dead")) || Packet.Message.Contains(TEXT("while alive")))
			{
				UE_LOG(LogConnection, Warning, TEXT("respawnResult: server says player is already alive — force-reviving client"));
				RespawnPlayer->SetDead_Implementation(false);
			}
		}
		return;
	}

	FString RequestIdEcho;
	const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
	if (Root->TryGetObjectField(TEXT("header"), HeaderPtr) && HeaderPtr && (*HeaderPtr).IsValid())
	{
		(*HeaderPtr)->TryGetStringField(TEXT("requestIdEcho"), RequestIdEcho);
	}
	if (RequestIdEcho.IsEmpty() && Body.IsValid())
	{
		Body->TryGetStringField(TEXT("requestIdEcho"), RequestIdEcho);
	}

	int32 BodyCharId = 0;
	if (Body.IsValid()) Body->TryGetNumberField(TEXT("characterId"), BodyCharId);
	const int32 ResolvedCharId = TeleportManager->ResolveCharacterId(BodyCharId, ClientData.clientId);

	FTeleportTargetData Target;
	if (!FTeleportParser::ParseRespawnResult(Body, RequestIdEcho, Target, ResolvedCharId))
	{
		return;
	}
	TeleportManager->ApplyServerTeleport(Target, true);
}

void UTeleportNetworkHandler::HandlePlayerTeleported(const FParsedPacket& Packet)
{
	const TSharedPtr<FJsonObject>& Body = Packet.Body;

	// Server-directed teleport (admin/GM/skill): unicast for the
	// locally-controlled actor. Remote views arrive as positional
	// broadcasts; this path also snaps a remote pawn without cover.
	FTeleportTargetData Target;
	if (!FTeleportParser::ParsePlayerTeleported(Body, Target))
	{
		return;
	}
	TeleportManager->ApplyServerTeleport(Target, false);
}
