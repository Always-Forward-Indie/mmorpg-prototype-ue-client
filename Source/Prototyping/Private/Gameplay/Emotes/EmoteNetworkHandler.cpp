#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UEmoteNetworkHandler::Initialize(UEmoteManager* InManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("EmoteNetworkHandler: Initialize called with null parameters"));
        return;
    }
    Manager          = InManager;
    NetworkManager   = InNetworkManager;
    GameInstance     = InGameInstance;
    LocalCharacterId = InGameInstance ? InGameInstance->GetCurrentCharacterID() : 0;
}

void UEmoteNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UEmoteNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UEmoteNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UEmoteNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

// ---------------------------------------------------------------------------
// Outbound
// ---------------------------------------------------------------------------

void UEmoteNetworkHandler::RequestUseEmote(const FString& EmoteSlug)
{
    if (!NetworkManager || !GameInstance || EmoteSlug.IsEmpty()) return;

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"),     MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    BodyData.Add(TEXT("emoteSlug"), MakeShareable(new FJsonValueString(EmoteSlug)));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(
        TEXT("useEmote"), HeaderData, BodyData,
        GameInstance->GetTimeSyncService(), EServerType::ChunkServer);

    NetworkManager->SendDataToChunkServer(JsonString);
    UE_LOG(LogTemp, Log, TEXT("EmoteNetworkHandler: Sent useEmote '%s'"), *EmoteSlug);
}

// ---------------------------------------------------------------------------
// Inbound
// ---------------------------------------------------------------------------

void UEmoteNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !Manager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);

    if (Msg.eventType == TEXT("player_emotes"))
    {
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
        if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
        {
            HandlePlayerEmotes(Root);
        }
        return;
    }

    if (Msg.eventType == TEXT("emoteAction"))
    {
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
        if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
        {
            HandleEmoteAction(Root);
        }
        return;
    }
}

void UEmoteNetworkHandler::HandlePlayerEmotes(const TSharedPtr<FJsonObject>& Root)
{
    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    FPlayerEmotesState State;
    Body->TryGetNumberField(TEXT("characterId"), State.characterId);

    // Filter to local character only
    if (LocalCharacterId > 0 && State.characterId > 0 && State.characterId != LocalCharacterId) return;

    const TArray<TSharedPtr<FJsonValue>>* EmotesArr = nullptr;
    if (Body->TryGetArrayField(TEXT("emotes"), EmotesArr))
    {
        for (const TSharedPtr<FJsonValue>& Val : *EmotesArr)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr)) continue;
            State.emotes.Add(ParseEmoteDefinition(*ObjPtr));
        }
    }

    Manager->ApplyPlayerEmotes(State);
}

void UEmoteNetworkHandler::HandleEmoteAction(const TSharedPtr<FJsonObject>& Root)
{
    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    int32  CharacterId   = 0;
    FString EmoteSlug;
    FString AnimationName;

    Body->TryGetNumberField(TEXT("characterId"),   CharacterId);
    Body->TryGetStringField(TEXT("emoteSlug"),     EmoteSlug);
    Body->TryGetStringField(TEXT("animationName"), AnimationName);

    if (CharacterId <= 0 || EmoteSlug.IsEmpty()) return;

    Manager->DispatchEmoteAction(CharacterId, EmoteSlug, AnimationName);
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

FEmoteDefinitionData UEmoteNetworkHandler::ParseEmoteDefinition(const TSharedPtr<FJsonObject>& Obj)
{
    FEmoteDefinitionData Def;
    Obj->TryGetNumberField(TEXT("id"),            Def.id);
    Obj->TryGetStringField(TEXT("slug"),          Def.slug);
    Obj->TryGetStringField(TEXT("displayName"),   Def.displayName);
    Obj->TryGetStringField(TEXT("animationName"), Def.animationName);
    Obj->TryGetStringField(TEXT("category"),      Def.category);
    Obj->TryGetBoolField  (TEXT("isDefault"),     Def.bIsDefault);
    Obj->TryGetNumberField(TEXT("sortOrder"),     Def.sortOrder);
    return Def;
}
