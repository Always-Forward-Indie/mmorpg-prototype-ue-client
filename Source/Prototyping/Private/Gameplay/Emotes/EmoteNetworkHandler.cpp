#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Prototyping.h"
#include "Services/TimeSyncService.h"
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
        UE_LOG(LogGameplay, Error, TEXT("EmoteNetworkHandler: Initialize called with null parameters"));
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
    FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
    Dispatcher.AddHandler(TEXT("player_emotes"), this, &UEmoteNetworkHandler::HandleChunkPacket);
    Dispatcher.AddHandler(TEXT("emoteAction"), this, &UEmoteNetworkHandler::HandleChunkPacket);
    bIsSubscribed = true;
}

void UEmoteNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
    Dispatcher.RemoveHandler(TEXT("player_emotes"), this);
    Dispatcher.RemoveHandler(TEXT("emoteAction"), this);
    bIsSubscribed = false;
}

// ---------------------------------------------------------------------------
// Outbound
// ---------------------------------------------------------------------------

void UEmoteNetworkHandler::RequestUseEmote(const FString& EmoteSlug)
{
    if (!NetworkManager || !GameInstance || EmoteSlug.IsEmpty()) return;

    const double Now = FPlatformTime::Seconds();
    if (Now - LastEmoteSendTime < EmoteCooldownSeconds) return;
    LastEmoteSendTime = Now;

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"),     MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    BodyData.Add(TEXT("emoteSlug"), MakeShareable(new FJsonValueString(EmoteSlug)));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(
        TEXT("useEmote"), HeaderData, BodyData,
        GameInstance->GetTimeSyncService(), EServerType::ChunkServer);

    NetworkManager->SendDataToChunkServer(JsonString);
    UE_LOG(LogGameplay, Verbose, TEXT("EmoteNetworkHandler: Sent useEmote '%s'"), *EmoteSlug);
}

// ---------------------------------------------------------------------------
// Inbound
// ---------------------------------------------------------------------------

void UEmoteNetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
    // Single-parse path (S05): EventType/Root come from PollLink, no re-parse.
    if (!Packet.bValid || !Packet.Root.IsValid() || !Manager) return;

    const TSharedPtr<FJsonObject>& Root = Packet.Root;

    if (Packet.EventType == TEXT("player_emotes"))
    {
        HandlePlayerEmotes(Root);
        return;
    }

    if (Packet.EventType == TEXT("emoteAction"))
    {
        HandleEmoteAction(Root);
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
