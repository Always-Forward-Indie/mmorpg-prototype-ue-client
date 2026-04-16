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
        UE_LOG(LogTemp, Error, TEXT("DialogueNetworkHandler: Cannot subscribe � NetworkManager is null"));
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
            ChoiceObj->TryGetBoolField  (TEXT("hideIfLocked"),    Choice.hideIfLocked);

            // questPreview (v0.0.5) — present when edge has offer_quest action
            const TSharedPtr<FJsonObject>* QPreviewPtr = nullptr;
            if (ChoiceObj->TryGetObjectField(TEXT("questPreview"), QPreviewPtr) && QPreviewPtr)
            {
                Choice.questPreview  = ParseQuestPreview(*QPreviewPtr);
                Choice.bHasQuestPreview = true;
            }

            // turnInPreview (v0.0.5) — present when edge has turn_in_quest action
            const TSharedPtr<FJsonObject>* TIPreviewPtr = nullptr;
            if (ChoiceObj->TryGetObjectField(TEXT("turnInPreview"), TIPreviewPtr) && TIPreviewPtr)
            {
                Choice.turnInPreview    = ParseQuestPreview(*TIPreviewPtr);
                Choice.bHasTurnInPreview = true;
            }

            // giftPreview (v0.0.6) — present when edge has give_item / give_gold / give_exp
            const TArray<TSharedPtr<FJsonValue>>* GiftArr = nullptr;
            if (ChoiceObj->TryGetArrayField(TEXT("giftPreview"), GiftArr) && GiftArr && !GiftArr->IsEmpty())
            {
                for (const TSharedPtr<FJsonValue>& GiftVal : *GiftArr)
                {
                    const TSharedPtr<FJsonObject>* GiftObjPtr;
                    if (!GiftVal->TryGetObject(GiftObjPtr)) continue;

                    FGiftPreviewItem Item;
                    (*GiftObjPtr)->TryGetStringField(TEXT("giftType"),  Item.giftType);
                    (*GiftObjPtr)->TryGetStringField(TEXT("item_slug"), Item.itemSlug);
                    (*GiftObjPtr)->TryGetNumberField(TEXT("quantity"),  Item.quantity);
                    (*GiftObjPtr)->TryGetNumberField(TEXT("amount"),    Item.amount);
                    Choice.giftPreview.Add(Item);
                }
                Choice.bHasGiftPreview = !Choice.giftPreview.IsEmpty();
            }

            Data.choices.Add(Choice);
        }
    }

    return Data;
}

FQuestPreviewData UDialogueNetworkHandler::ParseQuestPreview(
    const TSharedPtr<FJsonObject>& Obj) const
{
    FQuestPreviewData Preview;
    Obj->TryGetStringField(TEXT("questSlug"),      Preview.questSlug);
    Obj->TryGetStringField(TEXT("clientQuestKey"), Preview.clientQuestKey);

    // firstStep (for offer_quest preview) — absent for turn_in preview
    const TSharedPtr<FJsonObject>* StepPtr = nullptr;
    if (Obj->TryGetObjectField(TEXT("firstStep"), StepPtr) && StepPtr)
    {
        const TSharedPtr<FJsonObject>& StepObj = *StepPtr;
        StepObj->TryGetStringField(TEXT("clientStepKey"), Preview.firstStep.clientStepKey);
        StepObj->TryGetStringField(TEXT("stepType"),      Preview.firstStep.stepType);
        StepObj->TryGetStringField(TEXT("target_slug"),   Preview.firstStep.targetSlug);
        StepObj->TryGetStringField(TEXT("zone_slug"),     Preview.firstStep.zoneSlug);
        StepObj->TryGetNumberField(TEXT("x"),             Preview.firstStep.targetX);
        StepObj->TryGetNumberField(TEXT("y"),             Preview.firstStep.targetY);
        StepObj->TryGetNumberField(TEXT("count"),         Preview.firstStep.count);
        StepObj->TryGetNumberField(TEXT("current"),       Preview.firstStep.current);
        Preview.bHasFirstStep = true;
    }

    // rewards array
    const TArray<TSharedPtr<FJsonValue>>* RewardsArr = nullptr;
    if (Obj->TryGetArrayField(TEXT("rewards"), RewardsArr) && RewardsArr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *RewardsArr)
        {
            const TSharedPtr<FJsonObject>* RObjPtr;
            if (!Val->TryGetObject(RObjPtr)) continue;
            const TSharedPtr<FJsonObject>& RObj = *RObjPtr;

            FQuestRewardData R;
            RObj->TryGetStringField(TEXT("rewardType"), R.rewardType);
            RObj->TryGetBoolField  (TEXT("isHidden"),   R.isHidden);
            RObj->TryGetNumberField(TEXT("amount"),      R.amount);
            RObj->TryGetStringField(TEXT("item_slug"),   R.itemSlug);
            RObj->TryGetNumberField(TEXT("quantity"),    R.quantity);
            Preview.rewards.Add(R);
        }
    }

    return Preview;
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
