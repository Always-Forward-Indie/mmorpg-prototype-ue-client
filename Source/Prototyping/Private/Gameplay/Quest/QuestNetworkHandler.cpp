#include "Gameplay/Quest/QuestNetworkHandler.h"
#include "Gameplay/Quest/QuestNetworkHandler.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

UQuestNetworkHandler::UQuestNetworkHandler()
{
}

void UQuestNetworkHandler::Initialize(UQuestManager* InQuestManager, UNetworkManager* InNetworkManager)
{
    if (!InQuestManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("QuestNetworkHandler: Initialize called with null parameters"));
        return;
    }
    QuestManager   = InQuestManager;
    NetworkManager = InNetworkManager;
}

void UQuestNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("QuestNetworkHandler: Cannot subscribe – NetworkManager is null"));
        return;
    }
    if (bIsSubscribed)
    {
        return;
    }
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UQuestNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UQuestNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed)
    {
        return;
    }
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UQuestNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UQuestNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !QuestManager)
    {
        return;
    }

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    const FString& EventType = Msg.eventType;

    static const TArray<FString> HandledEvents = {
        TEXT("QUEST_UPDATE"), TEXT("quest_offered"), TEXT("quest_turned_in"),
        TEXT("exp_received"), TEXT("item_received"), TEXT("gold_received")
    };

    if (!HandledEvents.Contains(EventType))
    {
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("QuestNetworkHandler: Failed to parse JSON for event %s"), *EventType);
        return;
    }

    const TSharedPtr<FJsonObject>* BodyPtr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr))
    {
        return;
    }
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    if (EventType == TEXT("QUEST_UPDATE"))
    {
        FQuestProgressData Data = ParseQuestUpdate(Body);
        QuestManager->OnQuestUpdated(Data);
    }
    else if (EventType == TEXT("quest_offered"))
    {
        FQuestOfferedData Data = ParseQuestOffered(Body);
        QuestManager->OnQuestOffered(Data);
    }
    else if (EventType == TEXT("quest_turned_in"))
    {
        FQuestTurnedInData Data = ParseQuestTurnedIn(Body);
        QuestManager->OnQuestTurnedIn(Data);
    }
    else if (EventType == TEXT("exp_received"))
    {
        FExpReceivedData Data;
        Body->TryGetNumberField(TEXT("amount"), Data.amount);
        QuestManager->OnExpReceived(Data);
    }
    else if (EventType == TEXT("item_received"))
    {
        FItemReceivedData Data;
        Body->TryGetNumberField(TEXT("itemId"),   Data.itemId);
        Body->TryGetNumberField(TEXT("quantity"), Data.quantity);
        QuestManager->OnItemReceived(Data);
    }
    else if (EventType == TEXT("gold_received"))
    {
        FGoldReceivedData Data;
        Body->TryGetNumberField(TEXT("amount"), Data.amount);
        QuestManager->OnGoldReceived(Data);
    }
}

// ??? Parsers ??????????????????????????????????????????????????????????????????

FQuestProgressData UQuestNetworkHandler::ParseQuestUpdate(const TSharedPtr<FJsonObject>& Body) const
{
    FQuestProgressData Data;

    Body->TryGetNumberField(TEXT("questId"),     Data.questId);
    Body->TryGetStringField(TEXT("questSlug"),      Data.questSlug);
    Body->TryGetStringField(TEXT("clientQuestKey"), Data.clientQuestKey);
    Body->TryGetStringField(TEXT("state"),       Data.state);
    Body->TryGetNumberField(TEXT("currentStep"), Data.stepIndex);
    Body->TryGetNumberField(TEXT("totalSteps"),  Data.totalSteps);
    Body->TryGetStringField(TEXT("clientStepKey"), Data.clientStepKey);
    Body->TryGetStringField(TEXT("stepType"),    Data.stepType);
    Body->TryGetStringField(TEXT("completionMode"), Data.completionMode);

    // Serialize progress/required back to JSON string for raw storage
    const TSharedPtr<FJsonObject>* ProgressObjPtr = nullptr;
    const TSharedPtr<FJsonObject>* RequiredObjPtr = nullptr;

    bool bHasProgress = Body->TryGetObjectField(TEXT("progress"), ProgressObjPtr);
    bool bHasRequired = Body->TryGetObjectField(TEXT("required"), RequiredObjPtr);

    if (bHasProgress)
    {
        FString ProgressStr;
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ProgressStr);
        FJsonSerializer::Serialize((*ProgressObjPtr).ToSharedRef(), W);
        Data.progressJson = ProgressStr;
    }
    if (bHasRequired)
    {
        FString RequiredStr;
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&RequiredStr);
        FJsonSerializer::Serialize((*RequiredObjPtr).ToSharedRef(), W);
        Data.requiredJson = RequiredStr;
    }

    // Convenience int fields for kill/collect steps
    if (bHasProgress && bHasRequired)
    {
        ExtractStepProgress(
            bHasProgress ? *ProgressObjPtr : nullptr,
            bHasRequired ? *RequiredObjPtr : nullptr,
            Data.stepType,
            Data.progressCurrent,
            Data.progressRequired);
    }

    return Data;
}

FQuestOfferedData UQuestNetworkHandler::ParseQuestOffered(const TSharedPtr<FJsonObject>& Body) const
{
    FQuestOfferedData Data;
    Body->TryGetNumberField(TEXT("questId"),        Data.questId);
    Body->TryGetStringField(TEXT("clientQuestKey"), Data.clientQuestKey);
    return Data;
}

FQuestTurnedInData UQuestNetworkHandler::ParseQuestTurnedIn(const TSharedPtr<FJsonObject>& Body) const
{
    FQuestTurnedInData Data;
    Body->TryGetNumberField(TEXT("questId"),        Data.questId);
    Body->TryGetStringField(TEXT("clientQuestKey"), Data.clientQuestKey);
    return Data;
}

void UQuestNetworkHandler::ExtractStepProgress(const TSharedPtr<FJsonObject>& ProgressObj,
                                                const TSharedPtr<FJsonObject>& RequiredObj,
                                                const FString& StepType,
                                                int32& OutCurrent,
                                                int32& OutRequired) const
{
    OutCurrent  = 0;
    OutRequired = 0;

    if (!ProgressObj || !RequiredObj)
    {
        return;
    }

    if (StepType == TEXT("kill"))
    {
        ProgressObj->TryGetNumberField(TEXT("killed"), OutCurrent);
        RequiredObj->TryGetNumberField(TEXT("count"),  OutRequired);
    }
    else if (StepType == TEXT("collect"))
    {
        ProgressObj->TryGetNumberField(TEXT("have"),  OutCurrent);
        RequiredObj->TryGetNumberField(TEXT("count"), OutRequired);
    }
    else if (StepType == TEXT("talk") || StepType == TEXT("reach"))
    {
        bool bDone = false;
        ProgressObj->TryGetBoolField(TEXT("done"), bDone);
        OutCurrent  = bDone ? 1 : 0;
        OutRequired = 1;
    }
}
