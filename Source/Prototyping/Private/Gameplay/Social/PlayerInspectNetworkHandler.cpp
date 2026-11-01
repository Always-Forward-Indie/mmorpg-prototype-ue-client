#include "Gameplay/Social/PlayerInspectNetworkHandler.h"
#include "Prototyping.h"
#include "Services/TimeSyncService.h"
#include "Gameplay/Social/PlayerInspectManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"

void UPlayerInspectNetworkHandler::Initialize(UPlayerInspectManager* InManager, UNetworkManager* InNetworkManager)
{
	if (!InManager || !InNetworkManager)
	{
		UE_LOG(LogGameplay, Error, TEXT("PlayerInspectNetworkHandler: Initialize called with null parameters"));
		return;
	}
	InspectManager = InManager;
	NetworkManager = InNetworkManager;
}

void UPlayerInspectNetworkHandler::SubscribeToNetworkEvents()
{
	if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
	FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
	for (const FString& EventType : HandledEvents())
		Dispatcher.AddHandler(EventType, this, &UPlayerInspectNetworkHandler::HandleChunkPacket);
	bIsSubscribed = true;
}

void UPlayerInspectNetworkHandler::UnsubscribeFromNetworkEvents()
{
	if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
	FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
	for (const FString& EventType : HandledEvents())
		Dispatcher.RemoveHandler(EventType, this);
	bIsSubscribed = false;
}

const TArray<FString>& UPlayerInspectNetworkHandler::HandledEvents()
{
	static const TArray<FString> Events = {
		TEXT("playerInspect"), TEXT("playerInspectRequest"),
	};
	return Events;
}

void UPlayerInspectNetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
	if (!Packet.bValid || !InspectManager || !Packet.Body.IsValid()) return;

	if (Packet.EventType == TEXT("playerInspect"))
	{
		if (Packet.Status.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			FString Reason;
			Packet.Body->TryGetStringField(TEXT("reason"), Reason);
			if (Reason.IsEmpty())
				Reason = JSONParser::ResolveErrorCode(Packet.Root);
			if (Reason.IsEmpty())
				Reason = Packet.Message;
			InspectManager->OnInspectError(Reason);
			return;
		}
		InspectManager->OnInspectReceived(ParseInspect(Packet.Body));
	}
	else if (Packet.EventType == TEXT("playerInspectRequest")
		&& Packet.Status.Equals(TEXT("error"), ESearchCase::IgnoreCase))
	{
		// Request-echo errors (defensive: the server answers on playerInspect,
		// but any error echo must still surface instead of hanging the window).
		FString Reason;
		Packet.Body->TryGetStringField(TEXT("reason"), Reason);
		if (Reason.IsEmpty())
			Reason = JSONParser::ResolveErrorCode(Packet.Root);
		if (Reason.IsEmpty())
			Reason = Packet.Message;
		InspectManager->OnInspectError(Reason);
	}
}

FPlayerInspectData UPlayerInspectNetworkHandler::ParseInspect(const TSharedPtr<FJsonObject>& Body) const
{
	FPlayerInspectData Data;
	Body->TryGetNumberField(TEXT("targetCharacterId"), Data.CharacterId);
	Body->TryGetStringField(TEXT("characterName"), Data.CharacterName);
	Body->TryGetNumberField(TEXT("characterLevel"), Data.CharacterLevel);
	Body->TryGetStringField(TEXT("characterClass"), Data.ClassSlug);
	Body->TryGetStringField(TEXT("characterRace"), Data.RaceSlug);
	Body->TryGetStringField(TEXT("characterGender"), Data.GenderSlug);

	// slots: { slotSlug: {itemSlug, itemId} | null } — same shape as
	// PLAYER_EQUIPMENT_UPDATE entries (server buildEquipmentStateJson).
	const TSharedPtr<FJsonObject>* SlotsPtr = nullptr;
	if (Body->TryGetObjectField(TEXT("slots"), SlotsPtr) && SlotsPtr && (*SlotsPtr).IsValid())
	{
		for (const auto& Pair : (*SlotsPtr)->Values)
		{
			const TSharedPtr<FJsonValue>& Val = Pair.Value;
			if (!Val.IsValid() || Val->IsNull()) continue;
			const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
			if (!Val->TryGetObject(EntryPtr) || !EntryPtr || !(*EntryPtr).IsValid()) continue;
			const TSharedPtr<FJsonObject>& Entry = *EntryPtr;

			FPlayerInspectGearEntry Gear;
			Gear.SlotSlug = Pair.Key;
			Entry->TryGetStringField(TEXT("itemSlug"), Gear.ItemSlug);
			Entry->TryGetNumberField(TEXT("itemId"), Gear.ItemId);
			if (!Gear.ItemSlug.IsEmpty() || Gear.ItemId > 0)
				Data.Gear.Add(MoveTemp(Gear));
		}
	}
	return Data;
}
