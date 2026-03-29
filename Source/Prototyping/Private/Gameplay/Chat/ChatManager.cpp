#include "Gameplay/Chat/ChatManager.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
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
        UE_LOG(LogTemp, Error, TEXT("ChatManager: Initialize called with null parameters"));
        return;
    }
    GameInstance    = InGameInstance;
    NetworkManager  = InNetworkManager;
}

void UChatManager::SendChatMessage(const FString& Channel, const FString& Text, const FString& TargetName)
{
    if (!NetworkManager || !GameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("ChatManager: Cannot send message - missing dependencies"));
        return;
    }

    if (Text.IsEmpty() || Text.Len() > 255)
    {
        UE_LOG(LogTemp, Warning, TEXT("ChatManager: Message text is empty or exceeds 255 chars"));
        return;
    }

    if (Channel.Equals(TEXT("whisper"), ESearchCase::IgnoreCase) && TargetName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("ChatManager: Whisper requires a targetName"));
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
