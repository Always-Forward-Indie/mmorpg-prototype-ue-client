#include "Gameplay/Equipment/EquipmentNetworkHandler.h"
#include "Prototyping.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Services/TimeSyncService.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UEquipmentNetworkHandler::Initialize(UEquipmentManager* InEquipmentManager, UNetworkManager* InNetworkManager)
{
    if (!InEquipmentManager || !InNetworkManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("EquipmentNetworkHandler: Initialize called with null parameters"));
        return;
    }
    EquipmentManager = InEquipmentManager;
    NetworkManager   = InNetworkManager;
}

void UEquipmentNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
    for (const FString& EventType : HandledEvents())
    {
        Dispatcher.AddHandler(EventType, this, &UEquipmentNetworkHandler::HandleChunkPacket);
    }
    bIsSubscribed = true;
}

void UEquipmentNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    FNetworkDispatcher& Dispatcher = NetworkManager->DispatcherFor(EServerType::ChunkServer);
    for (const FString& EventType : HandledEvents())
    {
        Dispatcher.RemoveHandler(EventType, this);
    }
    bIsSubscribed = false;
}

void UEquipmentNetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
    if (!EquipmentManager) return;

    // Single-parse path (S05): EventType/Status come pre-parsed from the dispatcher.
    const FString& EventType = Packet.EventType;

    if (!HandledEvents().Contains(EventType)) return;

    // Single-parse path (S05): Root/Body come pre-parsed from the dispatcher.
    const TSharedPtr<FJsonObject>& Root = Packet.Root;
    if (!Root.IsValid()) return;

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
    else if (EventType == TEXT("useItem"))
    {
        // InventoryManager owns useItem success; surface only errors here
        // (e.g. body.reason == "cooldown") so they are not silently dropped.
        if (Packet.Status.Equals(TEXT("error"), ESearchCase::IgnoreCase))
        {
            FString UseReason;
            Body->TryGetStringField(TEXT("reason"), UseReason);
            if (UseReason.IsEmpty())
            {
                UseReason = JSONParser::ResolveErrorCode(Root);
            }
            UE_LOG(LogGameplay, Warning, TEXT("EquipmentNetworkHandler: useItem failed reason='%s'"), *UseReason);
            FEquipResultData UseItemError;
            UseItemError.bSuccess = false;
            UseItemError.errorCode = UseReason;
            EquipmentManager->OnEquipResultReceived(UseItemError);
        }
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

// EventTypes owned by this handler (single list drives dispatcher registration).
const TArray<FString>& UEquipmentNetworkHandler::HandledEvents()
{
    static const TArray<FString> Events = {
        TEXT("EQUIPMENT_STATE"), TEXT("EQUIP_RESULT"),
        TEXT("WEIGHT_STATUS"),   TEXT("charAttributesUpdate"),
        TEXT("PLAYER_EQUIPMENT_UPDATE"), TEXT("useItem")
    };
    return Events;
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
    Result.bSuccess = false;

    // Success is authoritative via header.status (success packets carry no header.message).
    FString Status;
    const TSharedPtr<FJsonObject>* EquipHeaderPtr = nullptr;
    if (Root->TryGetObjectField(TEXT("header"), EquipHeaderPtr) && EquipHeaderPtr)
    {
        (*EquipHeaderPtr)->TryGetStringField(TEXT("status"), Status);
    }
    const bool bIsSuccess = Status.Equals(TEXT("success"), ESearchCase::IgnoreCase)
        || Message.Equals(TEXT("success"), ESearchCase::IgnoreCase);

    if (!bIsSuccess)
    {
        // Machine-readable code: body.reason (protocol) wins, then body.errorCode/header.message.
        FString Reason;
        Body->TryGetStringField(TEXT("reason"), Reason);
        if (Reason.IsEmpty())
        {
            Reason = JSONParser::ResolveErrorCode(Root);
        }
        if (Reason.IsEmpty())
        {
            Reason = Message;
        }
        Result.errorCode = Reason;
        return Result;
    }
    Result.bSuccess = true;

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
