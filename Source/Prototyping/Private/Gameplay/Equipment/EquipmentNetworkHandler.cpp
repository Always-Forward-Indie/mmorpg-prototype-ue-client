#include "Gameplay/Equipment/EquipmentNetworkHandler.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UEquipmentNetworkHandler::Initialize(UEquipmentManager* InEquipmentManager, UNetworkManager* InNetworkManager)
{
    if (!InEquipmentManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("EquipmentNetworkHandler: Initialize called with null parameters"));
        return;
    }
    EquipmentManager = InEquipmentManager;
    NetworkManager   = InNetworkManager;
}

void UEquipmentNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UEquipmentNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UEquipmentNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UEquipmentNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UEquipmentNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !EquipmentManager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    const FString& EventType = Msg.eventType;

    static const TArray<FString> HandledEvents = {
        TEXT("EQUIPMENT_STATE"), TEXT("EQUIP_RESULT"),
        TEXT("WEIGHT_STATUS"),   TEXT("charAttributesUpdate"),
        TEXT("PLAYER_EQUIPMENT_UPDATE")
    };
    if (!HandledEvents.Contains(EventType)) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
    Root->TryGetObjectField(TEXT("header"), HeaderPtr);
    FString Message;
    if (HeaderPtr) (*HeaderPtr)->TryGetStringField(TEXT("message"), Message);

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    if (EventType == TEXT("EQUIPMENT_STATE"))
    {
        EquipmentManager->OnEquipmentStateReceived(ParseEquipmentState(Body));
    }
    else if (EventType == TEXT("EQUIP_RESULT"))
    {
        EquipmentManager->OnEquipResultReceived(ParseEquipResult(Root, Body, Message));
    }
    else if (EventType == TEXT("WEIGHT_STATUS"))
    {
        EquipmentManager->OnWeightStatusReceived(ParseWeightStatus(Body));
    }
    else if (EventType == TEXT("charAttributesUpdate"))
    {
        int32 CharacterId = 0;
        Body->TryGetNumberField(TEXT("characterId"), CharacterId);
        TArray<FAttributeDataStruct> Attrs = ParseAttributes(Body);
        EquipmentManager->OnAttributesUpdated(CharacterId, Attrs);
    }
    else if (EventType == TEXT("PLAYER_EQUIPMENT_UPDATE"))
    {
        EquipmentManager->OnRemoteEquipmentStateReceived(ParseEquipmentState(Body));
    }
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

FEquipmentStateData UEquipmentNetworkHandler::ParseEquipmentState(const TSharedPtr<FJsonObject>& Body) const
{
    FEquipmentStateData State;
    Body->TryGetNumberField(TEXT("characterId"), State.characterId);

    const TSharedPtr<FJsonObject>* SlotsPtr = nullptr;
    if (!Body->TryGetObjectField(TEXT("slots"), SlotsPtr)) return State;

    const TSharedPtr<FJsonObject>& SlotsObj = *SlotsPtr;
    for (const auto& SlotPair : SlotsObj->Values)
    {
        const FString SlugKey(SlotPair.Key);
        FEquipmentSlotData SlotData;

        if (SlotPair.Value->IsNull())
        {
            // null = empty slot, leave defaults (bIsOccupied = false)
            State.slots.Add(SlugKey, SlotData);
            continue;
        }

        TSharedPtr<FJsonObject> SlotObj = SlotPair.Value->AsObject();
        if (!SlotObj.IsValid())
        {
            State.slots.Add(SlugKey, SlotData);
            continue;
        }

        SlotObj->TryGetBoolField(TEXT("blockedByTwoHanded"), SlotData.blockedByTwoHanded);
        if (SlotData.blockedByTwoHanded)
        {
            // off_hand blocked � not occupied by an actual item
            State.slots.Add(SlugKey, SlotData);
            continue;
        }

        SlotObj->TryGetNumberField(TEXT("inventoryItemId"),  SlotData.inventoryItemId);
        SlotObj->TryGetNumberField(TEXT("itemId"),           SlotData.itemId);
        SlotObj->TryGetStringField(TEXT("itemSlug"),         SlotData.itemSlug);
        SlotObj->TryGetNumberField(TEXT("durabilityCurrent"),SlotData.durabilityCurrent);
        SlotObj->TryGetNumberField(TEXT("durabilityMax"),    SlotData.durabilityMax);
        SlotObj->TryGetBoolField  (TEXT("isDurabilityWarning"), SlotData.isDurabilityWarning);
        SlotData.bIsOccupied = (SlotData.inventoryItemId > 0);

        State.slots.Add(SlugKey, SlotData);
    }

    return State;
}

FEquipResultData UEquipmentNetworkHandler::ParseEquipResult(
    const TSharedPtr<FJsonObject>& Root,
    const TSharedPtr<FJsonObject>& Body,
    const FString& Message) const
{
    FEquipResultData Result;

    if (Message != TEXT("success"))
    {
        Result.errorCode = Message;
        return Result;
    }

    // Body contains either "equip" or "unequip" sub-object
    const TSharedPtr<FJsonObject>* ActionObjPtr = nullptr;
    TSharedPtr<FJsonObject> ActionObj;

    if (Body->TryGetObjectField(TEXT("equip"), ActionObjPtr))
    {
        ActionObj = *ActionObjPtr;
    }
    else if (Body->TryGetObjectField(TEXT("unequip"), ActionObjPtr))
    {
        ActionObj = *ActionObjPtr;
    }

    if (!ActionObj.IsValid()) return Result;

    ActionObj->TryGetStringField(TEXT("action"),             Result.action);
    ActionObj->TryGetNumberField(TEXT("inventoryItemId"),    Result.inventoryItemId);
    ActionObj->TryGetStringField(TEXT("equipSlotSlug"),      Result.equipSlotSlug);

    // swappedOutInventoryItemId can be null or an int
    if (!ActionObj->HasTypedField<EJson::Null>(TEXT("swappedOutInventoryItemId")))
    {
        ActionObj->TryGetNumberField(TEXT("swappedOutInventoryItemId"), Result.swappedOutInventoryItemId);
    }

    return Result;
}

FWeightStatusData UEquipmentNetworkHandler::ParseWeightStatus(const TSharedPtr<FJsonObject>& Body) const
{
    FWeightStatusData Status;
    Body->TryGetNumberField(TEXT("characterId"),   Status.characterId);
    Body->TryGetNumberField(TEXT("currentWeight"), Status.currentWeight);
    Body->TryGetNumberField(TEXT("weightLimit"),   Status.weightLimit);
    Body->TryGetBoolField  (TEXT("isOverweight"),  Status.isOverweight);
    return Status;
}

TArray<FAttributeDataStruct> UEquipmentNetworkHandler::ParseAttributes(const TSharedPtr<FJsonObject>& Body) const
{
    TArray<FAttributeDataStruct> Attrs;
    const TArray<TSharedPtr<FJsonValue>>* AttrsArray = nullptr;
    if (!Body->TryGetArrayField(TEXT("attributesData"), AttrsArray)) return Attrs;

    for (const TSharedPtr<FJsonValue>& Val : *AttrsArray)
    {
        TSharedPtr<FJsonObject> Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        FAttributeDataStruct Attr;
        Obj->TryGetNumberField(TEXT("id"),    Attr.attributeId);
        Obj->TryGetStringField(TEXT("slug"),  Attr.attributeSlug);
        Obj->TryGetNumberField(TEXT("value"), Attr.attributeValue);
        Attr.attributeName = Attr.attributeSlug; // display name resolved via LocalizationSubsystem
        Attrs.Add(Attr);
    }
    return Attrs;
}
