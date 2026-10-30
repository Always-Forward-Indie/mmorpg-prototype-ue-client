#include "Gameplay/Chat/ChatManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/ChatWhisperRule.h"
#include "Services/TimeSyncService.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UChatManager::UChatManager()
{
}

void UChatManager::Initialize(UMyGameInstance* InGameInstance, UNetworkManager* InNetworkManager)
{
    if (!InGameInstance || !InNetworkManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("ChatManager: Initialize called with null parameters"));
        return;
    }
    GameInstance    = InGameInstance;
    NetworkManager  = InNetworkManager;
}

void UChatManager::SendChatMessage(const FString& Channel, const FString& Text, const FString& TargetName)
{
    if (!NetworkManager || !GameInstance)
    {
        UE_LOG(LogGameplay, Error, TEXT("ChatManager: Cannot send message - missing dependencies"));
        return;
    }

    if (Text.IsEmpty() || Text.Len() > 255)
    {
        UE_LOG(LogGameplay, Warning, TEXT("ChatManager: Message text is empty or exceeds 255 chars"));
        return;
    }

    if (Channel.Equals(TEXT("whisper"), ESearchCase::IgnoreCase) && TargetName.IsEmpty())
    {
        UE_LOG(LogGameplay, Warning, TEXT("ChatManager: Whisper requires a targetName"));
        return;
    }

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    HeaderData.Add(TEXT("clientId"),  MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"),      MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    BodyData.Add(TEXT("channel"),     MakeShareable(new FJsonValueString(Channel)));
    BodyData.Add(TEXT("text"),        MakeShareable(new FJsonValueString(Text)));
    BodyData.Add(TEXT("targetName"),  MakeShareable(new FJsonValueString(TargetName)));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(
        TEXT("chatMessage"),
        HeaderData,
        BodyData,
        GameInstance->GetTimeSyncService(),
        EServerType::ChunkServer
    );

    NetworkManager->SendDataToChunkServer(JsonString);

    // Whispers are delivered to the target only — the server never echoes
    // them to the sender. Show our own line immediately (optimistic echo);
    // a "not found" error still arrives separately and renders red.
    if (FChatWhisperRule::NeedsLocalEcho(Channel) && GameInstance->Player)
    {
        FChatMessageStruct Echo;
        Echo.channel    = Channel;
        Echo.senderName = GameInstance->Player->GetPlayerData().characterData.characterName;
        Echo.senderId   = GameInstance->GetCurrentCharacterID();
        Echo.text       = Text;
        if (UTimeSyncService* Sync = GameInstance->GetTimeSyncService())
        {
            Echo.timestamp = Sync->GetEstimatedServerTimeMs();
        }
        OnMessageReceived(Echo);
    }
}

void UChatManager::OnMessageReceived(const FChatMessageStruct& Message)
{
    MessageLog.Add(Message);

    // Trim oldest entries if log exceeds the cap
    while (MessageLog.Num() > MaxLogSize)
    {
        MessageLog.RemoveAt(0);
    }

    OnChatMessageReceived.Broadcast(Message);
}
