#include "Gameplay/Trade/TradeNetworkHandler.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UTradeNetworkHandler::Initialize(UTradeManager* InTradeManager, UNetworkManager* InNetworkManager)
{
    if (!InTradeManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("TradeNetworkHandler: Initialize called with null parameters"));
        return;
    }
    TradeManager   = InTradeManager;
    NetworkManager = InNetworkManager;
}

void UTradeNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UTradeNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UTradeNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UTradeNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UTradeNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !TradeManager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    const FString& EventType = Msg.eventType;

    static const TArray<FString> HandledEvents = {
        TEXT("tradeInvite"), TEXT("tradeState"),   TEXT("tradeDeclined"),
        TEXT("tradeCancelled"), TEXT("tradeComplete")
    };
    if (!HandledEvents.Contains(EventType)) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    if      (EventType == TEXT("tradeInvite"))    TradeManager->OnTradeInviteReceived(ParseTradeInvite(Body));
    else if (EventType == TEXT("tradeState"))     TradeManager->OnTradeStateReceived(ParseTradeState(Body));
    else if (EventType == TEXT("tradeDeclined"))  TradeManager->OnTradeDeclined(ParseTradeDeclined(Body));
    else if (EventType == TEXT("tradeCancelled")) TradeManager->OnTradeCancelled(ParseTradeCancelled(Body));
    else if (EventType == TEXT("tradeComplete"))  TradeManager->OnTradeCompleted(ParseTradeComplete(Body));
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

FTradeInviteData UTradeNetworkHandler::ParseTradeInvite(const TSharedPtr<FJsonObject>& Body) const
{
    FTradeInviteData Data;
    Body->TryGetNumberField(TEXT("fromCharacterId"),   Data.initiatorId);
    Body->TryGetStringField(TEXT("fromCharacterName"), Data.fromCharacterName);
    Body->TryGetStringField(TEXT("sessionId"),         Data.sessionId);
    return Data;
}

FTradeStateData UTradeNetworkHandler::ParseTradeState(const TSharedPtr<FJsonObject>& Body) const
{
    FTradeStateData State;
    const TSharedPtr<FJsonObject>* TradePtr = nullptr;
    if (!Body->TryGetObjectField(TEXT("trade"), TradePtr)) return State;
    const TSharedPtr<FJsonObject>& Trade = *TradePtr;

    Trade->TryGetStringField(TEXT("sessionId"),      State.sessionId);
    Trade->TryGetNumberField(TEXT("myGold"),         State.myGold);
    Trade->TryGetNumberField(TEXT("myGoldBalance"),  State.myGoldBalance);
    Trade->TryGetNumberField(TEXT("theirGold"),      State.theirGold);
    Trade->TryGetBoolField  (TEXT("myConfirmed"),    State.myConfirmed);
    Trade->TryGetBoolField  (TEXT("theirConfirmed"), State.theirConfirmed);

    // Parse a trade item array as full FInventoryItemStruct objects
    // (server sends the same object format as getPlayerInventory items)
    auto ParseInventoryItems = [](const TSharedPtr<FJsonObject>& Obj, const FString& Key, TArray<FInventoryItemStruct>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
        if (!Obj->TryGetArrayField(Key, Arr)) return;
        for (const TSharedPtr<FJsonValue>& Val : *Arr)
        {
            TSharedPtr<FJsonObject> ItemObj = Val->AsObject();
            if (!ItemObj.IsValid()) continue;
            FInventoryItemStruct Item;
            ItemObj->TryGetNumberField(TEXT("id"),              Item.id);
            ItemObj->TryGetNumberField(TEXT("itemId"),          Item.itemId);
            ItemObj->TryGetNumberField(TEXT("quantity"),        Item.quantity);
            ItemObj->TryGetNumberField(TEXT("slotIndex"),       Item.slotIndex);
            ItemObj->TryGetStringField(TEXT("slug"),            Item.slug);
            ItemObj->TryGetNumberField(TEXT("itemType"),        Item.item_type_id);
            ItemObj->TryGetStringField(TEXT("itemTypeSlug"),    Item.itemTypeSlug);
            ItemObj->TryGetNumberField(TEXT("rarityId"),        Item.rarityId);
            ItemObj->TryGetStringField(TEXT("raritySlug"),      Item.raritySlug);
            ItemObj->TryGetNumberField(TEXT("durabilityMax"),   Item.durabilityMax);
            if (!ItemObj->TryGetNumberField(TEXT("durabilityCurrent"), Item.durabilityCurrent))
                Item.durabilityCurrent = Item.durabilityMax;
            ItemObj->TryGetBoolField  (TEXT("isDurable"),       Item.isDurable);
            ItemObj->TryGetBoolField  (TEXT("isTradable"),      Item.isTradable);
            ItemObj->TryGetBoolField  (TEXT("isEquippable"),    Item.isEquippable);
            ItemObj->TryGetBoolField  (TEXT("isUsable"),        Item.isUsable);
            ItemObj->TryGetBoolField  (TEXT("isTwoHanded"),     Item.isTwoHanded);
            ItemObj->TryGetBoolField  (TEXT("isEquipped"),      Item.is_equipped);
            ItemObj->TryGetBoolField  (TEXT("isQuestItem"),     Item.isQuestItem);
            ItemObj->TryGetBoolField  (TEXT("isContainer"),     Item.isContainer);
            ItemObj->TryGetBoolField  (TEXT("isHarvest"),       Item.isHarvestItem);
            ItemObj->TryGetNumberField(TEXT("equipSlot"),       Item.equip_slot_id);
            ItemObj->TryGetStringField(TEXT("equipSlotSlug"),   Item.equipSlotSlug);
            ItemObj->TryGetNumberField(TEXT("levelRequirement"),Item.level_requirement);
            ItemObj->TryGetNumberField(TEXT("setId"),           Item.set_id);
            ItemObj->TryGetStringField(TEXT("setSlug"),         Item.setSlug);
            ItemObj->TryGetStringField(TEXT("masterySlug"),     Item.masterySlug);
            ItemObj->TryGetNumberField(TEXT("killCount"),       Item.killCount);

            double WeightVal = 0.0;
            if (ItemObj->TryGetNumberField(TEXT("weight"), WeightVal))
                Item.weight = static_cast<float>(WeightVal);

            // attributes: [{slug, value}]
            const TArray<TSharedPtr<FJsonValue>>* AttrsArray = nullptr;
            if (ItemObj->TryGetArrayField(TEXT("attributes"), AttrsArray) && AttrsArray)
            {
                for (const TSharedPtr<FJsonValue>& AttrVal : *AttrsArray)
                {
                    TSharedPtr<FJsonObject> AttrObj = AttrVal->AsObject();
                    if (!AttrObj.IsValid()) continue;
                    FString AttrSlug;
                    AttrObj->TryGetStringField(TEXT("slug"), AttrSlug);
                    FString ValueStr;
                    double NumVal = 0.0;
                    if (AttrObj->TryGetNumberField(TEXT("value"), NumVal))
                        ValueStr = FString::SanitizeFloat(NumVal);
                    else
                        AttrObj->TryGetStringField(TEXT("value"), ValueStr);
                    if (!AttrSlug.IsEmpty())
                        Item.attributes.Add(AttrSlug, ValueStr);
                }
            }

            // useEffects
            const TArray<TSharedPtr<FJsonValue>>* EffectsArray = nullptr;
            if (ItemObj->TryGetArrayField(TEXT("useEffects"), EffectsArray) && EffectsArray)
            {
                for (const TSharedPtr<FJsonValue>& EffVal : *EffectsArray)
                {
                    TSharedPtr<FJsonObject> EffObj = EffVal->AsObject();
                    if (!EffObj.IsValid()) continue;
                    FItemUseEffectEntry Effect;
                    EffObj->TryGetStringField(TEXT("effectSlug"),     Effect.effectSlug);
                    EffObj->TryGetStringField(TEXT("attributeSlug"),  Effect.attributeSlug);
                    EffObj->TryGetBoolField  (TEXT("isInstant"),      Effect.isInstant);
                    EffObj->TryGetNumberField(TEXT("durationSeconds"),Effect.durationSeconds);
                    EffObj->TryGetNumberField(TEXT("tickMs"),         Effect.tickMs);
                    EffObj->TryGetNumberField(TEXT("cooldownSeconds"),Effect.cooldownSeconds);
                    double EffValue = 0.0;
                    if (EffObj->TryGetNumberField(TEXT("value"), EffValue))
                        Effect.value = static_cast<float>(EffValue);
                    Item.useEffects.Add(Effect);
                }
            }

            Out.Add(Item);
        }
    };

    ParseInventoryItems(Trade, TEXT("myItems"),    State.myItems);
    ParseInventoryItems(Trade, TEXT("theirItems"), State.theirItems);

    return State;
}

FTradeDeclinedData UTradeNetworkHandler::ParseTradeDeclined(const TSharedPtr<FJsonObject>& Body) const
{
    FTradeDeclinedData Data;
    Body->TryGetStringField(TEXT("byCharacterName"), Data.byCharacterName);
    return Data;
}

FTradeCancelledData UTradeNetworkHandler::ParseTradeCancelled(const TSharedPtr<FJsonObject>& Body) const
{
    FTradeCancelledData Data;
    Body->TryGetStringField(TEXT("sessionId"), Data.sessionId);
    Body->TryGetStringField(TEXT("reason"),    Data.reason);
    return Data;
}

FTradeCompleteData UTradeNetworkHandler::ParseTradeComplete(const TSharedPtr<FJsonObject>& Body) const
{
    FTradeCompleteData Data;
    Body->TryGetStringField(TEXT("sessionId"),      Data.sessionId);
    Body->TryGetNumberField(TEXT("receivedGold"),   Data.receivedGold);
    Body->TryGetNumberField(TEXT("newGoldBalance"), Data.newGoldBalance);

    const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
    if (Body->TryGetArrayField(TEXT("receivedItems"), ItemsArray) && ItemsArray)
    {
        for (const TSharedPtr<FJsonValue>& Val : *ItemsArray)
        {
            TSharedPtr<FJsonObject> Obj = Val->AsObject();
            if (!Obj.IsValid()) continue;
            FTradeReceivedItem Item;
            Obj->TryGetNumberField(TEXT("itemId"),   Item.itemId);
            Obj->TryGetStringField(TEXT("slug"),     Item.slug);
            Obj->TryGetNumberField(TEXT("quantity"), Item.quantity);
            Data.receivedItems.Add(Item);
        }
    }
    return Data;
}
