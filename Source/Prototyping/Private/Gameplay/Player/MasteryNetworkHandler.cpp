#include "Gameplay/Player/MasteryNetworkHandler.h"
#include "Gameplay/Player/MasteryManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UMasteryNetworkHandler::Initialize(UMasteryManager* InManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("MasteryNetworkHandler: Initialize called with null parameters"));
        return;
    }
    Manager        = InManager;
    NetworkManager = InNetworkManager;
    GameInstance   = InGameInstance;
    LocalCharacterId = InGameInstance ? InGameInstance->GetCurrentCharacterID() : 0;
}

void UMasteryNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UMasteryNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UMasteryNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UMasteryNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UMasteryNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !Manager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    if (Msg.eventType != TEXT("player_masteries") && Msg.eventType != TEXT("mastery_update")) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;

    if (Msg.eventType == TEXT("player_masteries"))
    {
        FPlayerMasteriesState State = ParseMasteriesState(*BodyPtr);
        if (LocalCharacterId > 0 && State.characterId > 0 && State.characterId != LocalCharacterId) return;
        Manager->ApplyMasteriesState(State);
    }
    else // mastery_update
    {
        FMasteryUpdateData Update = ParseMasteryUpdate(*BodyPtr);
        if (LocalCharacterId > 0 && Update.characterId > 0 && Update.characterId != LocalCharacterId) return;
        Manager->ApplyMasteryUpdate(Update);
    }
}

FPlayerMasteriesState UMasteryNetworkHandler::ParseMasteriesState(const TSharedPtr<FJsonObject>& Body) const
{
    FPlayerMasteriesState State;
    Body->TryGetNumberField(TEXT("characterId"), State.characterId);

    const TArray<TSharedPtr<FJsonValue>>* EntriesArr = nullptr;
    if (Body->TryGetArrayField(TEXT("entries"), EntriesArr))
    {
        for (const TSharedPtr<FJsonValue>& Val : *EntriesArr)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr)) continue;

            FMasteryEntry Entry;
            (*ObjPtr)->TryGetStringField(TEXT("masterySlug"), Entry.masterySlug);
            double Tmp = 0.0;
            if ((*ObjPtr)->TryGetNumberField(TEXT("value"), Tmp)) Entry.value = static_cast<float>(Tmp);
            State.entries.Add(Entry);
        }
    }
    return State;
}

FMasteryUpdateData UMasteryNetworkHandler::ParseMasteryUpdate(const TSharedPtr<FJsonObject>& Body) const
{
    FMasteryUpdateData Update;
    Body->TryGetNumberField(TEXT("characterId"), Update.characterId);
    Body->TryGetStringField(TEXT("masterySlug"), Update.masterySlug);
    double Tmp = 0.0;
    if (Body->TryGetNumberField(TEXT("value"), Tmp)) Update.value = static_cast<float>(Tmp);
    return Update;
}
