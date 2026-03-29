#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UDialogueManager::UDialogueManager()
{
}

void UDialogueManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InNetworkManager || !InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("DialogueManager: Initialize called with null parameters"));
        return;
    }
    NetworkManager  = InNetworkManager;
    GameInstance    = InGameInstance;
    UE_LOG(LogTemp, Log, TEXT("DialogueManager: Initialized"));
}

// ??? Client ? Server ??????????????????????????????????????????????????????????

void UDialogueManager::OpenDialogue(int32 NpcId)
{
    if (!NetworkManager || !GameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("DialogueManager::OpenDialogue: not initialized"));
        return;
    }

    if (bSessionActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("DialogueManager::OpenDialogue: a session is already active (%s), closing first"), *CurrentSessionId);
        CloseDialogue();
    }

    CurrentNpcId = NpcId;

    const int32 ClientId     = GameInstance->GetCurrentClientID();
    const int32 CharacterId  = GameInstance->GetCurrentCharacterID();
    const FString Hash       = GameInstance->GetCurrentClientHash();

    // Build packet
    TSharedPtr<FJsonObject> Root  = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("npcInteract"));
    Header->SetNumberField(TEXT("clientId"),  ClientId);
    Header->SetStringField(TEXT("hash"),      Hash);

    Body->SetNumberField(TEXT("npcId"),       NpcId);
    Body->SetNumberField(TEXT("characterId"), CharacterId);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    NetworkManager->SendDataToChunkServer(Payload);

    UE_LOG(LogTemp, Log, TEXT("DialogueManager: Sent npcInteract for NPC %d"), NpcId);
}

void UDialogueManager::SendChoice(int32 EdgeId)
{
    if (!bSessionActive || CurrentSessionId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("DialogueManager::SendChoice: no active session"));
        return;
    }
    if (!NetworkManager || !GameInstance)
    {
        return;
    }

    const int32 ClientId    = GameInstance->GetCurrentClientID();
    const int32 CharacterId = GameInstance->GetCurrentCharacterID();
    const FString Hash      = GameInstance->GetCurrentClientHash();

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("dialogueChoice"));
    Header->SetNumberField(TEXT("clientId"),  ClientId);
    Header->SetStringField(TEXT("hash"),      Hash);

    Body->SetStringField(TEXT("sessionId"),   CurrentSessionId);
    Body->SetNumberField(TEXT("edgeId"),      EdgeId);
    Body->SetNumberField(TEXT("characterId"), CharacterId);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    NetworkManager->SendDataToChunkServer(Payload);

    UE_LOG(LogTemp, Log, TEXT("DialogueManager: Sent dialogueChoice edgeId=%d session=%s"), EdgeId, *CurrentSessionId);
}

void UDialogueManager::CloseDialogue()
{
    if (!bSessionActive || CurrentSessionId.IsEmpty())
    {
        return;
    }
    if (!NetworkManager || !GameInstance)
    {
        return;
    }

    const int32 ClientId    = GameInstance->GetCurrentClientID();
    const int32 CharacterId = GameInstance->GetCurrentCharacterID();
    const FString Hash      = GameInstance->GetCurrentClientHash();

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("dialogueClose"));
    Header->SetNumberField(TEXT("clientId"),  ClientId);
    Header->SetStringField(TEXT("hash"),      Hash);

    Body->SetStringField(TEXT("sessionId"),   CurrentSessionId);
    Body->SetNumberField(TEXT("characterId"), CharacterId);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    NetworkManager->SendDataToChunkServer(Payload);

    UE_LOG(LogTemp, Log, TEXT("DialogueManager: Sent dialogueClose session=%s"), *CurrentSessionId);

    // Clear local state immediately
    bSessionActive    = false;
    CurrentSessionId  = "";
    CurrentNpcId      = 0;
    CurrentNode       = FDialogueNodeData();
}

// ??? Called by DialogueNetworkHandler ?????????????????????????????????????????

void UDialogueManager::OnNodeReceived(const FDialogueNodeData& NodeData)
{
    bSessionActive   = true;
    CurrentSessionId = NodeData.sessionId;
    CurrentNpcId     = NodeData.npcId;
    CurrentNode      = NodeData;

    UE_LOG(LogTemp, Log, TEXT("DialogueManager: Node received type=%s key=%s session=%s"),
        *NodeData.type, *NodeData.clientNodeKey, *NodeData.sessionId);

    OnDialogueNodeReceived.Broadcast(NodeData);
}

void UDialogueManager::OnSessionClosed(const FString& SessionId)
{
    UE_LOG(LogTemp, Log, TEXT("DialogueManager: Session closed: %s"), *SessionId);

    bSessionActive    = false;
    CurrentSessionId  = "";
    CurrentNpcId      = 0;
    CurrentNode       = FDialogueNodeData();

    OnDialogueSessionClosed.Broadcast(SessionId);
}

void UDialogueManager::OnErrorReceived(const FDialogueErrorData& ErrorData)
{
    UE_LOG(LogTemp, Warning, TEXT("DialogueManager: Error received: %s — %s"),
        *ErrorData.errorCode, *ErrorData.message);

    OnDialogueError.Broadcast(ErrorData);
}
