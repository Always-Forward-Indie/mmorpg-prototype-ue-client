#include "Gameplay/Player/TitleNetworkHandler.h"
#include "Gameplay/Player/TitleManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Services/LocalizationSubsystem.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UTitleNetworkHandler::Initialize(UTitleManager* InManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("TitleNetworkHandler: Initialize called with null parameters"));
        return;
    }
    Manager          = InManager;
    NetworkManager   = InNetworkManager;
    GameInstance     = InGameInstance;
    LocalCharacterId = InGameInstance ? InGameInstance->GetCurrentCharacterID() : 0;
}

void UTitleNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UTitleNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UTitleNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UTitleNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

// ---------------------------------------------------------------------------
// Outbound requests
// ---------------------------------------------------------------------------

void UTitleNetworkHandler::RequestGetTitles(int32 CharacterId)
{
    if (!NetworkManager || !GameInstance) return;

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"),     MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    BodyData.Add(TEXT("characterId"), MakeShareable(new FJsonValueNumber(CharacterId)));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(
        TEXT("getTitles"), HeaderData, BodyData,
        GameInstance->GetTimeSyncService(), EServerType::ChunkServer);

    NetworkManager->SendDataToChunkServer(JsonString);
    UE_LOG(LogTemp, Log, TEXT("TitleNetworkHandler: Sent getTitles for character %d"), CharacterId);
}

void UTitleNetworkHandler::RequestEquipTitle(int32 CharacterId, const FString& TitleSlug)
{
    if (!NetworkManager || !GameInstance) return;

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"),     MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    BodyData.Add(TEXT("characterId"), MakeShareable(new FJsonValueNumber(CharacterId)));
    BodyData.Add(TEXT("titleSlug"),   MakeShareable(new FJsonValueString(TitleSlug)));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(
        TEXT("equipTitle"), HeaderData, BodyData,
        GameInstance->GetTimeSyncService(), EServerType::ChunkServer);

    NetworkManager->SendDataToChunkServer(JsonString);
    UE_LOG(LogTemp, Log, TEXT("TitleNetworkHandler: Sent equipTitle='%s' for character %d"), *TitleSlug, CharacterId);
}

// ---------------------------------------------------------------------------
// Inbound: player_titles_update
// ---------------------------------------------------------------------------

void UTitleNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !Manager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    if (Msg.eventType == TEXT("player_titles_update"))
    {
        HandleOwnTitlesUpdate(Root);
    }
    else if (Msg.eventType == TEXT("PLAYER_TITLE_CHANGED"))
    {
        HandleRemoteTitleChanged(Root);
    }
}

void UTitleNetworkHandler::HandleOwnTitlesUpdate(const TSharedPtr<FJsonObject>& Root)
{
    // Check for error status
    FString Status;
    if (const TSharedPtr<FJsonObject>* HeaderPtr = nullptr; Root->TryGetObjectField(TEXT("header"), HeaderPtr))
        (*HeaderPtr)->TryGetStringField(TEXT("status"), Status);
    if (!Status.IsEmpty() && Status != TEXT("success")) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;

    FPlayerTitlesState State = ParseTitlesState(*BodyPtr);

    if (LocalCharacterId > 0 && State.characterId > 0 && State.characterId != LocalCharacterId) return;

    Manager->ApplyTitlesState(State);
}

void UTitleNetworkHandler::HandleRemoteTitleChanged(const TSharedPtr<FJsonObject>& Root)
{
    if (!GameInstance) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;

    int32 CharacterId = 0;
    (*BodyPtr)->TryGetNumberField(TEXT("characterId"), CharacterId);
    if (CharacterId <= 0) return;

    FString TitleSlug;
    FString TitleDisplayName;
    (*BodyPtr)->TryGetStringField(TEXT("equippedTitleSlug"),       TitleSlug);
    (*BodyPtr)->TryGetStringField(TEXT("equippedTitleDisplayName"), TitleDisplayName);

    // Prefer display name; fall back to localized name, then slug so the nameplate always shows something.
    FString TitleText = TitleDisplayName;
    if (TitleText.IsEmpty() && !TitleSlug.IsEmpty())
    {
        if (const ULocalizationSubsystem* Loc = GameInstance->GetSubsystem<ULocalizationSubsystem>())
        {
            TitleText = Loc->GetTitleDisplayName(TitleSlug).ToString();
        }
    }
    if (TitleText.IsEmpty())
        TitleText = TitleSlug;

    ABasicPlayer* RemotePlayer = GameInstance->GetPlayerByCharacterId(CharacterId);
    if (!RemotePlayer || !IsValid(RemotePlayer)) return;

    RemotePlayer->SetEquippedTitle(TitleText);
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

FTitleEntry UTitleNetworkHandler::ParseTitleEntry(const TSharedPtr<FJsonObject>& Obj)
{
    FTitleEntry Entry;
    Obj->TryGetStringField(TEXT("slug"),          Entry.slug);
    Obj->TryGetStringField(TEXT("displayName"),   Entry.displayName);
    Obj->TryGetStringField(TEXT("description"),   Entry.description);
    Obj->TryGetStringField(TEXT("earnCondition"), Entry.earnCondition);

    // Fall back to localized display name if server didn't provide one
    if (Entry.displayName.IsEmpty() && !Entry.slug.IsEmpty() && GameInstance)
    {
        if (const ULocalizationSubsystem* Loc = GameInstance->GetSubsystem<ULocalizationSubsystem>())
        {
            Entry.displayName = Loc->GetTitleDisplayName(Entry.slug).ToString();
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* BonusesArr = nullptr;
    if (Obj->TryGetArrayField(TEXT("bonuses"), BonusesArr))
    {
        for (const TSharedPtr<FJsonValue>& Val : *BonusesArr)
        {
            const TSharedPtr<FJsonObject>* BonusObj = nullptr;
            if (!Val->TryGetObject(BonusObj)) continue;
            FTitleBonusEntry Bonus;
            (*BonusObj)->TryGetStringField(TEXT("attributeSlug"), Bonus.attributeSlug);
            double Tmp = 0.0;
            if ((*BonusObj)->TryGetNumberField(TEXT("value"), Tmp)) Bonus.value = static_cast<float>(Tmp);
            Entry.bonuses.Add(Bonus);
        }
    }
    return Entry;
}

FPlayerTitlesState UTitleNetworkHandler::ParseTitlesState(const TSharedPtr<FJsonObject>& Body)
{
    FPlayerTitlesState State;
    Body->TryGetNumberField(TEXT("characterId"), State.characterId);

    // Server sends { "equippedTitle": { "slug": ..., "displayName": ... } } or null/absent.
    // There is no flat "equippedTitleSlug" key — derive it from the nested object's slug.
    if (const TSharedPtr<FJsonObject>* EquippedObj = nullptr; Body->TryGetObjectField(TEXT("equippedTitle"), EquippedObj))
    {
        State.equippedTitle = ParseTitleEntry(*EquippedObj);
        State.equippedTitleSlug = State.equippedTitle.slug;
    }

    // earnedTitles array
    const TArray<TSharedPtr<FJsonValue>>* EarnedArr = nullptr;
    if (Body->TryGetArrayField(TEXT("earnedTitles"), EarnedArr))
    {
        for (const TSharedPtr<FJsonValue>& Val : *EarnedArr)
        {
            const TSharedPtr<FJsonObject>* TitleObj = nullptr;
            if (!Val->TryGetObject(TitleObj)) continue;
            State.earnedTitles.Add(ParseTitleEntry(*TitleObj));
        }
    }
    return State;
}
