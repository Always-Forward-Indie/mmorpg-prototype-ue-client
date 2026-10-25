#include "Gameplay/Chat/ChatNetworkHandler.h"
#include "Prototyping.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

UChatNetworkHandler::UChatNetworkHandler()
{
}

void UChatNetworkHandler::Initialize(UChatManager* InChatManager, UNetworkManager* InNetworkManager)
{
    if (!InChatManager || !InNetworkManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("ChatNetworkHandler: Initialize called with null parameters"));
        return;
    }
    ChatManager    = InChatManager;
    NetworkManager = InNetworkManager;
}

void UChatNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        UE_LOG(LogGameplay, Error, TEXT("ChatNetworkHandler: Cannot subscribe - NetworkManager is null"));
        return;
    }
    if (bIsSubscribed)
    {
        return;
    }
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UChatNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UChatNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed)
    {
        return;
    }
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UChatNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UChatNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !ChatManager)
    {
        return;
    }

    // Quick check before full parse
    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    if (Msg.eventType != TEXT("chatMessage"))
    {
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogNetPacket, Error, TEXT("ChatNetworkHandler: Failed to parse JSON"));
        return;
    }

    FChatMessageStruct Message = ParseChatMessage(Root);
    ChatManager->OnMessageReceived(Message);
}

FChatMessageStruct UChatNetworkHandler::ParseChatMessage(const TSharedPtr<FJsonObject>& Root) const
{
    FChatMessageStruct Message;

    // Whisper errors arrive with header.status=error. New servers put the code in
    // body.errorCode (body may be null on old servers) — header.message is the fallback.
    const FString ErrorCode = JSONParser::ResolveErrorCode(Root);
    if (!ErrorCode.IsEmpty() && !ErrorCode.Equals(TEXT("success"), ESearchCase::IgnoreCase))
    {
        Message.errorMessage = ErrorCode;
        Message.channel      = TEXT("whisper");
        Message.bIsError     = true;
        return Message;
    }

    // Normal broadcast
    const TSharedPtr<FJsonObject>* BodyPtr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr->IsValid())
    {
        return Message;
    }
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    Body->TryGetStringField(TEXT("channel"),    Message.channel);
    Body->TryGetStringField(TEXT("senderName"), Message.senderName);
    Body->TryGetNumberField(TEXT("senderId"),   Message.senderId);
    Body->TryGetStringField(TEXT("text"),       Message.text);
    Body->TryGetNumberField(TEXT("timestamp"),  Message.timestamp);

    return Message;
}
