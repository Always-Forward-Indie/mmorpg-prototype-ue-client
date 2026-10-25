#include "Gameplay/Player/ReputationNetworkHandler.h"
#include "Prototyping.h"
#include "Services/TimeSyncService.h"
#include "Gameplay/Player/ReputationManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UReputationNetworkHandler::Initialize(UReputationManager* InManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InManager || !InNetworkManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("ReputationNetworkHandler: Initialize called with null parameters"));
        return;
    }
    Manager          = InManager;
    NetworkManager   = InNetworkManager;
    GameInstance     = InGameInstance;
    LocalCharacterId = InGameInstance ? InGameInstance->GetCurrentCharacterID() : 0;
}

void UReputationNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
    Dispatcher.AddHandler(TEXT("player_reputations"), this, &UReputationNetworkHandler::HandleChunkPacket);
    Dispatcher.AddHandler(TEXT("reputation_update"), this, &UReputationNetworkHandler::HandleChunkPacket);
    bIsSubscribed = true;
}

void UReputationNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
    Dispatcher.RemoveHandler(TEXT("player_reputations"), this);
    Dispatcher.RemoveHandler(TEXT("reputation_update"), this);
    bIsSubscribed = false;
}

void UReputationNetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
    // Single-parse path (S05): EventType/Body come from PollLink, no re-parse.
    if (!Packet.bValid || !Manager || !Packet.Body.IsValid()) return;

    const FString& EventType = Packet.EventType;
    const TSharedPtr<FJsonObject>& Body = Packet.Body;

    if (EventType == TEXT("player_reputations"))
    {
        FPlayerReputationsState State = ParseReputationsState(Body);
        if (LocalCharacterId > 0 && State.characterId > 0 && State.characterId != LocalCharacterId) return;
        Manager->ApplyReputationsState(State);
    }
    else // reputation_update
    {
        FReputationUpdateData Update = ParseReputationUpdate(Body);
        if (LocalCharacterId > 0 && Update.characterId > 0 && Update.characterId != LocalCharacterId) return;
        Manager->ApplyReputationUpdate(Update);
    }
}

FPlayerReputationsState UReputationNetworkHandler::ParseReputationsState(const TSharedPtr<FJsonObject>& Body) const
{
    FPlayerReputationsState State;
    Body->TryGetNumberField(TEXT("characterId"), State.characterId);

    const TArray<TSharedPtr<FJsonValue>>* EntriesArr = nullptr;
    if (Body->TryGetArrayField(TEXT("entries"), EntriesArr))
    {
        for (const TSharedPtr<FJsonValue>& Val : *EntriesArr)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr)) continue;

            FReputationEntry Entry;
            (*ObjPtr)->TryGetStringField(TEXT("factionSlug"), Entry.factionSlug);
            (*ObjPtr)->TryGetNumberField(TEXT("value"),       Entry.value);
            (*ObjPtr)->TryGetStringField(TEXT("tier"),        Entry.tier);
            State.entries.Add(Entry);
        }
    }
    return State;
}

FReputationUpdateData UReputationNetworkHandler::ParseReputationUpdate(const TSharedPtr<FJsonObject>& Body) const
{
    FReputationUpdateData Update;
    Body->TryGetNumberField(TEXT("characterId"), Update.characterId);
    Body->TryGetStringField(TEXT("factionSlug"), Update.factionSlug);
    Body->TryGetNumberField(TEXT("value"),       Update.value);
    Body->TryGetStringField(TEXT("tier"),        Update.tier);
    return Update;
}
