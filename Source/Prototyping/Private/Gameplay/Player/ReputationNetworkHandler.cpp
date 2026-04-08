#include "Gameplay/Player/ReputationNetworkHandler.h"
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
        UE_LOG(LogTemp, Error, TEXT("ReputationNetworkHandler: Initialize called with null parameters"));
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
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UReputationNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UReputationNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UReputationNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UReputationNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !Manager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    if (Msg.eventType != TEXT("player_reputations") && Msg.eventType != TEXT("reputation_update")) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;

    if (Msg.eventType == TEXT("player_reputations"))
    {
        FPlayerReputationsState State = ParseReputationsState(*BodyPtr);
        if (LocalCharacterId > 0 && State.characterId > 0 && State.characterId != LocalCharacterId) return;
        Manager->ApplyReputationsState(State);
    }
    else // reputation_update
    {
        FReputationUpdateData Update = ParseReputationUpdate(*BodyPtr);
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
