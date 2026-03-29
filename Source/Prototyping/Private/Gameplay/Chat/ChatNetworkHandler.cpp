#include "Gameplay/Chat/ChatNetworkHandler.h"
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
        UE_LOG(LogTemp, Error, TEXT("ChatNetworkHandler: Initialize called with null parameters"));
        return;
    }
    ChatManager    = InChatManager;
    NetworkManager = InNetworkManager;
}

void UChatNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("ChatNetworkHandler: Cannot subscribe - NetworkManager is null"));
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
        UE_LOG(LogTemp, Error, TEXT("ChatNetworkHandler: Failed to parse JSON"));
        return;
    }

    FChatMessageStruct Message = ParseChatMessage(Root);
    ChatManager->OnMessageReceived(Message);
}

FChatMessageStruct UChatNetworkHandler::ParseChatMessage(const TSharedPtr<FJsonObject>& Root) const
{
    FChatMessageStruct Message;

    // Check for a whisper error: body is null but header.message is non-empty
    const TSharedPtr<FJsonObject>* HeaderPtr;
    if (Root->TryGetObjectField(TEXT("header"), HeaderPtr))
    {
        FString HeaderMessage;
        if ((*HeaderPtr)->TryGetStringField(TEXT("message"), HeaderMessage) && !HeaderMessage.IsEmpty()
            && !HeaderMessage.Equals(TEXT("success"), ESearchCase::IgnoreCase))
        {
            Message.errorMessage = HeaderMessage;
            Message.channel      = TEXT("whisper");
            return Message;
        }
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
