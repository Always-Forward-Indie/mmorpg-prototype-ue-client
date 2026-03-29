#include "Gameplay/Vendor/VendorNetworkHandler.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UVendorNetworkHandler::Initialize(UVendorManager* InVendorManager, UNetworkManager* InNetworkManager)
{
    if (!InVendorManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("VendorNetworkHandler: Initialize called with null parameters"));
        return;
    }
    VendorManager  = InVendorManager;
    NetworkManager = InNetworkManager;
}

void UVendorNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UVendorNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UVendorNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UVendorNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UVendorNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !VendorManager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    const FString& EventType = Msg.eventType;

    static const TArray<FString> HandledEvents = {
        TEXT("openVendorShop"), TEXT("vendorShop"),
        TEXT("buyItemResult"),  TEXT("sellItemResult"),
        TEXT("buyItemBatchResult"), TEXT("sellItemBatchResult")
    };
    if (!HandledEvents.Contains(EventType)) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    FString Message;
    const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
    if (Root->TryGetObjectField(TEXT("header"), HeaderPtr))
    {
        (*HeaderPtr)->TryGetStringField(TEXT("message"), Message);
    }

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    if (EventType == TEXT("openVendorShop") || EventType == TEXT("vendorShop"))
    {
        FVendorShopData ShopData = ParseVendorShop(Body);
        UE_LOG(LogTemp, Warning, TEXT("VendorNetworkHandler: parsed shop npcId=%d items=%d"), ShopData.npcId, ShopData.items.Num());
        VendorManager->OnVendorShopReceived(ShopData);
    }
    else if (EventType == TEXT("buyItemResult"))
    {
        VendorManager->OnBuyItemResultReceived(ParseBuyItemResult(Body, Message));
    }
    else if (EventType == TEXT("sellItemResult"))
    {
        VendorManager->OnSellItemResultReceived(ParseSellItemResult(Body, Message));
    }
    else if (EventType == TEXT("buyItemBatchResult"))
    {
        VendorManager->OnBuyItemBatchResultReceived(ParseBuyItemBatchResult(Body, Message));
    }
    else if (EventType == TEXT("sellItemBatchResult"))
    {
        VendorManager->OnSellItemBatchResultReceived(ParseSellItemBatchResult(Body, Message));
    }
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

FVendorShopData UVendorNetworkHandler::ParseVendorShop(const TSharedPtr<FJsonObject>& Body) const
{
    FVendorShopData Shop;
    Body->TryGetNumberField(TEXT("npcId"),        Shop.npcId);
    Body->TryGetStringField(TEXT("npcName"),      Shop.npcName);
    Body->TryGetStringField(TEXT("npcSlug"),      Shop.npcSlug);
    Body->TryGetNumberField(TEXT("goldBalance"),  Shop.goldBalance);

    const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
    if (!Body->TryGetArrayField(TEXT("items"), ItemsArray)) return Shop;

    for (const TSharedPtr<FJsonValue>& Val : *ItemsArray)
    {
        TSharedPtr<FJsonObject> Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        FVendorShopItemData Item;
        Obj->TryGetNumberField(TEXT("itemId"),         Item.itemId);
        Obj->TryGetStringField(TEXT("slug"),           Item.slug);
        Obj->TryGetNumberField(TEXT("itemType"),       Item.itemType);
        Obj->TryGetStringField(TEXT("itemTypeSlug"),   Item.itemTypeSlug);
        Obj->TryGetNumberField(TEXT("rarityId"),       Item.rarityId);
        Obj->TryGetStringField(TEXT("raritySlug"),     Item.raritySlug);
        Obj->TryGetNumberField(TEXT("stackMax"),       Item.stackMax);
        Obj->TryGetBoolField  (TEXT("isDurable"),      Item.isDurable);
        Obj->TryGetNumberField(TEXT("durabilityMax"),  Item.durabilityMax);
        Obj->TryGetBoolField  (TEXT("isTradable"),     Item.isTradable);
        Obj->TryGetBoolField  (TEXT("isEquippable"),   Item.isEquippable);
        Obj->TryGetBoolField  (TEXT("isUsable"),       Item.isUsable);
        Obj->TryGetBoolField  (TEXT("isTwoHanded"),    Item.isTwoHanded);
        Obj->TryGetBoolField  (TEXT("isQuestItem"),    Item.isQuestItem);
        Obj->TryGetBoolField  (TEXT("isContainer"),    Item.isContainer);
        Obj->TryGetBoolField  (TEXT("isHarvest"),      Item.isHarvest);

        double WeightVal = 0.0;
        if (Obj->TryGetNumberField(TEXT("weight"), WeightVal))
            Item.weight = static_cast<float>(WeightVal);

        Obj->TryGetNumberField(TEXT("equipSlot"),      Item.equipSlot);
        Obj->TryGetStringField(TEXT("equipSlotSlug"),  Item.equipSlotSlug);
        Obj->TryGetNumberField(TEXT("levelRequirement"), Item.levelRequirement);
        Obj->TryGetNumberField(TEXT("setId"),          Item.setId);
        Obj->TryGetStringField(TEXT("setSlug"),        Item.setSlug);
        Obj->TryGetStringField(TEXT("masterySlug"),    Item.masterySlug);
        Obj->TryGetNumberField(TEXT("killCount"),      Item.killCount);
        Obj->TryGetNumberField(TEXT("priceBuy"),       Item.priceBuy);
        Obj->TryGetNumberField(TEXT("priceSell"),      Item.priceSell);
        Obj->TryGetNumberField(TEXT("stockCurrent"),   Item.stockCurrent);
        Obj->TryGetNumberField(TEXT("stockMax"),       Item.stockMax);

        // allowedClassIds
        const TArray<TSharedPtr<FJsonValue>>* ClassIds = nullptr;
        if (Obj->TryGetArrayField(TEXT("allowedClassIds"), ClassIds) && ClassIds)
        {
            for (const TSharedPtr<FJsonValue>& CIdVal : *ClassIds)
            {
                int32 CId = 0;
                if (CIdVal->TryGetNumber(CId))
                    Item.allowedClassIds.Add(CId);
            }
        }

        // attributes: [{slug, value}]
        const TArray<TSharedPtr<FJsonValue>>* AttrsArray = nullptr;
        if (Obj->TryGetArrayField(TEXT("attributes"), AttrsArray) && AttrsArray)
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
        if (Obj->TryGetArrayField(TEXT("useEffects"), EffectsArray) && EffectsArray)
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

        Shop.items.Add(Item);
    }
    return Shop;
}

FBuyItemResultData UVendorNetworkHandler::ParseBuyItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const
{
    FBuyItemResultData Result;
    if (Message != TEXT("success"))
    {
        Result.errorCode = Message;
        return Result;
    }
    Body->TryGetNumberField(TEXT("npcId"),          Result.npcId);
    Body->TryGetNumberField(TEXT("itemId"),         Result.itemId);
    Body->TryGetNumberField(TEXT("quantity"),       Result.quantity);
    Body->TryGetNumberField(TEXT("goldSpent"),      Result.goldSpent);
    Body->TryGetNumberField(TEXT("totalPrice"),     Result.totalPrice); // legacy
    // Prefer goldSpent; totalPrice is the legacy field name
    if (Result.goldSpent == 0 && Result.totalPrice > 0)
        Result.goldSpent = Result.totalPrice;
    Body->TryGetNumberField(TEXT("newGoldBalance"), Result.newGoldBalance);
    return Result;
}

FSellItemResultData UVendorNetworkHandler::ParseSellItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const
{
    FSellItemResultData Result;
    if (Message != TEXT("success"))
    {
        Result.errorCode = Message;
        return Result;
    }
    Body->TryGetNumberField(TEXT("npcId"),           Result.npcId);
    Body->TryGetNumberField(TEXT("inventorySlotId"), Result.inventorySlotId);
    Body->TryGetNumberField(TEXT("quantity"),        Result.quantity);
    Body->TryGetNumberField(TEXT("goldReceived"),    Result.goldReceived);
    Body->TryGetNumberField(TEXT("newGoldBalance"),  Result.newGoldBalance);
    return Result;
}

FBuyItemBatchResultData UVendorNetworkHandler::ParseBuyItemBatchResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const
{
    FBuyItemBatchResultData Result;
    if (Message != TEXT("success"))
    {
        Result.errorCode = Message;
        return Result;
    }
    Body->TryGetNumberField(TEXT("totalGoldSpent"), Result.totalGoldSpent);

    const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
    if (Body->TryGetArrayField(TEXT("items"), ItemsArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *ItemsArray)
        {
            TSharedPtr<FJsonObject> Obj = Val->AsObject();
            if (!Obj.IsValid()) continue;
            FBuyBatchItemResult Item;
            Obj->TryGetNumberField(TEXT("itemId"),     Item.itemId);
            Obj->TryGetNumberField(TEXT("quantity"),   Item.quantity);
            Obj->TryGetNumberField(TEXT("totalPrice"), Item.totalPrice);
            Result.items.Add(Item);
        }
    }
    return Result;
}

FSellItemBatchResultData UVendorNetworkHandler::ParseSellItemBatchResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const
{
    FSellItemBatchResultData Result;
    if (Message != TEXT("success"))
    {
        Result.errorCode = Message;
        return Result;
    }
    Body->TryGetNumberField(TEXT("totalGoldReceived"), Result.totalGoldReceived);

    const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
    if (Body->TryGetArrayField(TEXT("items"), ItemsArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *ItemsArray)
        {
            TSharedPtr<FJsonObject> Obj = Val->AsObject();
            if (!Obj.IsValid()) continue;
            FSellBatchItemResult Item;
            Obj->TryGetNumberField(TEXT("inventoryItemId"), Item.inventoryItemId);
            Obj->TryGetNumberField(TEXT("quantity"),        Item.quantity);
            Obj->TryGetNumberField(TEXT("goldReceived"),    Item.goldReceived);
            Result.items.Add(Item);
        }
    }
    return Result;
}
