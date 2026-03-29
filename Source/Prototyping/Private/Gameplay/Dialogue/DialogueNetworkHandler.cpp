#include "Gameplay/Dialogue/DialogueNetworkHandler.h"
#include "Gameplay/Dialogue/DialogueNetworkHandler.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

UDialogueNetworkHandler::UDialogueNetworkHandler()
{
}

void UDialogueNetworkHandler::Initialize(UDialogueManager* InDialogueManager, UNetworkManager* InNetworkManager)
{
    if (!InDialogueManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("DialogueNetworkHandler: Initialize called with null parameters"));
        return;
    }
    DialogueManager = InDialogueManager;
    NetworkManager  = InNetworkManager;
}

void UDialogueNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("DialogueNetworkHandler: Cannot subscribe – NetworkManager is null"));
        return;
    }
    if (bIsSubscribed)
    {
        return;
    }
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UDialogueNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UDialogueNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed)
    {
        return;
    }
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UDialogueNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UDialogueNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !DialogueManager)
    {
        return;
    }

    // Quick event type check before full parse
    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    const FString& EventType = Msg.eventType;

    if (EventType != TEXT("DIALOGUE_NODE") &&
        EventType != TEXT("DIALOGUE_CLOSE") &&
        EventType != TEXT("dialogueError"))
    {
        return;
    }

    // Full JSON parse
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("DialogueNetworkHandler: Failed to parse JSON for event %s"), *EventType);
        return;
    }

    if (EventType == TEXT("DIALOGUE_NODE"))
    {
        FDialogueNodeData NodeData = ParseDialogueNode(Root);
        DialogueManager->OnNodeReceived(NodeData);
    }
    else if (EventType == TEXT("DIALOGUE_CLOSE"))
    {
        FString SessionId;
        const TSharedPtr<FJsonObject>* BodyPtr;
        if (Root->TryGetObjectField(TEXT("body"), BodyPtr))
        {
            (*BodyPtr)->TryGetStringField(TEXT("sessionId"), SessionId);
        }
        DialogueManager->OnSessionClosed(SessionId);
    }
    else if (EventType == TEXT("dialogueError"))
    {
        FDialogueErrorData ErrorData = ParseDialogueError(Root);
        DialogueManager->OnErrorReceived(ErrorData);
    }
}

FDialogueNodeData UDialogueNetworkHandler::ParseDialogueNode(const TSharedPtr<FJsonObject>& Root) const
{
    FDialogueNodeData Data;

    const TSharedPtr<FJsonObject>* BodyPtr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr))
    {
        return Data;
    }
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    Body->TryGetStringField(TEXT("sessionId"),      Data.sessionId);
    Body->TryGetNumberField(TEXT("npcId"),           Data.npcId);
    Body->TryGetNumberField(TEXT("nodeId"),          Data.nodeId);
    Body->TryGetStringField(TEXT("clientNodeKey"),   Data.clientNodeKey);
    Body->TryGetStringField(TEXT("type"),            Data.type);
    Body->TryGetNumberField(TEXT("speakerNpcId"),    Data.speakerNpcId);

    const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
    if (Body->TryGetArrayField(TEXT("choices"), ChoicesArray))
    {
        for (const TSharedPtr<FJsonValue>& ChoiceVal : *ChoicesArray)
        {
            const TSharedPtr<FJsonObject>* ChoiceObjPtr;
            if (!ChoiceVal->TryGetObject(ChoiceObjPtr))
            {
                continue;
            }
            const TSharedPtr<FJsonObject>& ChoiceObj = *ChoiceObjPtr;

            FDialogueChoice Choice;
            ChoiceObj->TryGetNumberField(TEXT("edgeId"),          Choice.edgeId);
            ChoiceObj->TryGetStringField(TEXT("clientChoiceKey"), Choice.clientChoiceKey);
            ChoiceObj->TryGetBoolField  (TEXT("conditionMet"),    Choice.conditionMet);

            Data.choices.Add(Choice);
        }
    }

    return Data;
}

FDialogueErrorData UDialogueNetworkHandler::ParseDialogueError(const TSharedPtr<FJsonObject>& Root) const
{
    FDialogueErrorData Data;

    // message lives in header
    const TSharedPtr<FJsonObject>* HeaderPtr;
    if (Root->TryGetObjectField(TEXT("header"), HeaderPtr))
    {
        (*HeaderPtr)->TryGetStringField(TEXT("message"), Data.message);
    }

    const TSharedPtr<FJsonObject>* BodyPtr;
    if (Root->TryGetObjectField(TEXT("body"), BodyPtr))
    {
        (*BodyPtr)->TryGetStringField(TEXT("errorCode"), Data.errorCode);
        // Only present for BLOCKED_BY_REPUTATION errors
        (*BodyPtr)->TryGetStringField(TEXT("factionSlug"), Data.factionSlug);
    }

    return Data;
}
