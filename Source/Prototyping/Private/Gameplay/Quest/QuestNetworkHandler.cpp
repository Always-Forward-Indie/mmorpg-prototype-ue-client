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
        UE_LOG(LogTemp, Error, TEXT("QuestNetworkHandler: Cannot subscribe � NetworkManager is null"));
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
        TEXT("quest_failed"), TEXT("exp_received"), TEXT("item_received"),
        TEXT("gold_received"), TEXT("reputationChanged")
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
    else if (EventType == TEXT("quest_failed"))
    {
        FQuestFailedData Data;
        Body->TryGetNumberField(TEXT("questId"),      Data.questId);
        Body->TryGetStringField(TEXT("clientQuestKey"), Data.clientQuestKey);
        QuestManager->OnQuestFailed(Data);
    }
    else if (EventType == TEXT("reputationChanged"))
    {
        FReputationChangedData Data;
        Body->TryGetStringField(TEXT("faction"), Data.faction);
        Body->TryGetNumberField(TEXT("delta"),   Data.delta);
        QuestManager->OnReputationChanged(Data);
    }
}

// ??? Parsers ??????????????????????????????????????????????????????????????????

FQuestProgressData UQuestNetworkHandler::ParseQuestUpdate(const TSharedPtr<FJsonObject>& Body) const
{
    FQuestProgressData Data;

    Body->TryGetNumberField(TEXT("questId"),        Data.questId);
    Body->TryGetStringField(TEXT("questSlug"),      Data.questSlug);
    Body->TryGetStringField(TEXT("clientQuestKey"), Data.clientQuestKey);
    Body->TryGetStringField(TEXT("state"),          Data.state);
    Body->TryGetNumberField(TEXT("currentStep"),    Data.stepIndex);
    Body->TryGetNumberField(TEXT("totalSteps"),     Data.totalSteps);
    Body->TryGetStringField(TEXT("clientStepKey"),  Data.clientStepKey);
    Body->TryGetStringField(TEXT("stepType"),       Data.stepType);
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

    // currentStepEnriched (v0.0.5)
    const TSharedPtr<FJsonObject>* EnrichedPtr = nullptr;
    if (Body->TryGetObjectField(TEXT("currentStepEnriched"), EnrichedPtr) && EnrichedPtr)
    {
        Data.currentStepEnriched = ParseEnrichedStep(*EnrichedPtr);
        Data.bHasEnrichedStep = true;
    }

    // rewards (v0.0.5)
    const TArray<TSharedPtr<FJsonValue>>* RewardsArr = nullptr;
    if (Body->TryGetArrayField(TEXT("rewards"), RewardsArr) && RewardsArr)
    {
        Data.rewards = ParseRewards(*RewardsArr);
    }

    return Data;
}

FQuestOfferedData UQuestNetworkHandler::ParseQuestOffered(const TSharedPtr<FJsonObject>& Body) const
{
    FQuestOfferedData Data;
    Body->TryGetNumberField(TEXT("questId"),        Data.questId);
    Body->TryGetStringField(TEXT("clientQuestKey"), Data.clientQuestKey);

    // currentStep enriched data (v0.0.5)
    const TSharedPtr<FJsonObject>* StepPtr = nullptr;
    if (Body->TryGetObjectField(TEXT("currentStep"), StepPtr) && StepPtr)
    {
        Data.currentStep    = ParseEnrichedStep(*StepPtr);
        Data.bHasCurrentStep = true;
    }

    // rewards (v0.0.5)
    const TArray<TSharedPtr<FJsonValue>>* RewardsArr = nullptr;
    if (Body->TryGetArrayField(TEXT("rewards"), RewardsArr) && RewardsArr)
    {
        Data.rewards = ParseRewards(*RewardsArr);
    }

    return Data;
}

FQuestTurnedInData UQuestNetworkHandler::ParseQuestTurnedIn(const TSharedPtr<FJsonObject>& Body) const
{
    FQuestTurnedInData Data;
    Body->TryGetNumberField(TEXT("questId"),        Data.questId);
    Body->TryGetStringField(TEXT("clientQuestKey"), Data.clientQuestKey);

    // rewardsReceived — fully revealed (v0.0.5)
    const TArray<TSharedPtr<FJsonValue>>* RewardsArr = nullptr;
    if (Body->TryGetArrayField(TEXT("rewardsReceived"), RewardsArr) && RewardsArr)
    {
        Data.rewardsReceived = ParseRewards(*RewardsArr);
    }

    return Data;
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

TArray<FQuestRewardData> UQuestNetworkHandler::ParseRewards(
    const TArray<TSharedPtr<FJsonValue>>& Arr) const
{
    TArray<FQuestRewardData> Result;
    for (const TSharedPtr<FJsonValue>& Val : Arr)
    {
        const TSharedPtr<FJsonObject>* ObjPtr;
        if (!Val->TryGetObject(ObjPtr)) continue;
        const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

        FQuestRewardData R;
        Obj->TryGetStringField(TEXT("rewardType"), R.rewardType);
        Obj->TryGetBoolField  (TEXT("isHidden"),   R.isHidden);
        Obj->TryGetNumberField(TEXT("amount"),      R.amount);
        Obj->TryGetStringField(TEXT("item_slug"),   R.itemSlug);
        Obj->TryGetNumberField(TEXT("quantity"),    R.quantity);
        Result.Add(R);
    }
    return Result;
}

FQuestStepEnrichedData UQuestNetworkHandler::ParseEnrichedStep(
    const TSharedPtr<FJsonObject>& Obj) const
{
    FQuestStepEnrichedData S;
    Obj->TryGetStringField(TEXT("clientStepKey"), S.clientStepKey);
    Obj->TryGetStringField(TEXT("stepType"),      S.stepType);
    Obj->TryGetStringField(TEXT("target_slug"),   S.targetSlug);
    Obj->TryGetStringField(TEXT("zone_slug"),     S.zoneSlug);
    Obj->TryGetNumberField(TEXT("x"),             S.targetX);
    Obj->TryGetNumberField(TEXT("y"),             S.targetY);
    Obj->TryGetNumberField(TEXT("count"),         S.count);
    Obj->TryGetNumberField(TEXT("current"),       S.current);

    // params for custom step type
    const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
    if (Obj->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
    {
        FString ParamsStr;
        TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ParamsStr);
        FJsonSerializer::Serialize((*ParamsPtr).ToSharedRef(), W);
        S.paramsJson = ParamsStr;
    }
    return S;
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
