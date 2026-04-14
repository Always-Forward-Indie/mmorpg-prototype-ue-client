#include "Gameplay/Repair/RepairNetworkHandler.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void URepairNetworkHandler::Initialize(URepairManager* InRepairManager, UNetworkManager* InNetworkManager)
{
    if (!InRepairManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("RepairNetworkHandler: Initialize called with null parameters"));
        return;
    }
    RepairManager  = InRepairManager;
    NetworkManager = InNetworkManager;
}

void URepairNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &URepairNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void URepairNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &URepairNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void URepairNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty() || !RepairManager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);
    const FString& EventType = Msg.eventType;

    static const TArray<FString> HandledEvents = {
        TEXT("openRepairShop"), TEXT("repairShop"), TEXT("repairItemResult"), TEXT("repairAllResult")
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

    if (EventType == TEXT("openRepairShop") || EventType == TEXT("repairShop"))
    {
        RepairManager->OnRepairShopReceived(ParseRepairShop(Body));
    }
    else if (EventType == TEXT("repairItemResult"))
    {
        RepairManager->OnRepairItemResultReceived(ParseRepairItemResult(Body, Message));
    }
    else if (EventType == TEXT("repairAllResult"))
    {
        RepairManager->OnRepairAllResultReceived(ParseRepairAllResult(Body, Message));
    }
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

FRepairShopData URepairNetworkHandler::ParseRepairShop(const TSharedPtr<FJsonObject>& Body) const
{
    FRepairShopData Shop;
    Body->TryGetNumberField(TEXT("npcId"),        Shop.npcId);
    Body->TryGetNumberField(TEXT("goldBalance"),  Shop.goldBalance);

    const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
    if (!Body->TryGetArrayField(TEXT("items"), ItemsArray)) return Shop;

    int32 AccumulatedCost = 0;
    for (const TSharedPtr<FJsonValue>& Val : *ItemsArray)
    {
        TSharedPtr<FJsonObject> Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        FRepairShopItemData Item;
        Obj->TryGetNumberField(TEXT("inventoryItemId"),  Item.inventoryItemId);
        Obj->TryGetNumberField(TEXT("inventorySlotId"),  Item.inventoryItemId); // protocol uses inventorySlotId
        Obj->TryGetNumberField(TEXT("itemId"),           Item.itemId);
        // Server sends "itemName"; legacy clients may send "slug".  Populate both and
        // use itemName as a fallback so the row widget always has a name to display.
        Obj->TryGetStringField(TEXT("slug"),     Item.slug);
        Obj->TryGetStringField(TEXT("itemName"), Item.itemName);
        if (Item.slug.IsEmpty()) Item.slug = Item.itemName;
        Obj->TryGetNumberField(TEXT("durabilityMax"),    Item.durabilityMax);
        Obj->TryGetNumberField(TEXT("durabilityCurrent"),Item.durabilityCurrent);
        Obj->TryGetNumberField(TEXT("repairCost"),       Item.repairCost);
        AccumulatedCost += Item.repairCost;
        Shop.items.Add(Item);
    }

    // Prefer the server-provided total; fall back to sum of item costs
    if (!Body->TryGetNumberField(TEXT("repairAllCost"), Shop.repairAllCost) || Shop.repairAllCost == 0)
    {
        Shop.repairAllCost = AccumulatedCost;
    }
    // Keep legacy alias in sync
    Shop.totalRepairCost = Shop.repairAllCost;
    return Shop;
}

FRepairItemResultData URepairNetworkHandler::ParseRepairItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const
{
    FRepairItemResultData Result;
    if (Message != TEXT("success"))
    {
        Result.errorCode = Message;
        return Result;
    }
    Body->TryGetNumberField(TEXT("inventoryItemId"),  Result.inventoryItemId);
    Body->TryGetNumberField(TEXT("itemId"),           Result.itemId);
    Body->TryGetNumberField(TEXT("durabilityMax"),    Result.durabilityMax);
    Body->TryGetNumberField(TEXT("goldSpent"),        Result.goldSpent);
    Body->TryGetNumberField(TEXT("newGoldBalance"),   Result.newGoldBalance);
    return Result;
}

FRepairAllResultData URepairNetworkHandler::ParseRepairAllResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const
{
    FRepairAllResultData Result;
    if (Message != TEXT("success"))
    {
        Result.errorCode = Message;
        return Result;
    }
    Body->TryGetNumberField(TEXT("goldSpent"),      Result.goldSpent);
    Body->TryGetNumberField(TEXT("newGoldBalance"), Result.newGoldBalance);

    const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
    if (!Body->TryGetArrayField(TEXT("repairedItems"), ItemsArray)) return Result;

    for (const TSharedPtr<FJsonValue>& Val : *ItemsArray)
    {
        TSharedPtr<FJsonObject> Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;
        FRepairedItemEntry Entry;
        Obj->TryGetNumberField(TEXT("inventoryItemId"), Entry.inventoryItemId);
        Obj->TryGetNumberField(TEXT("itemId"),          Entry.itemId);
        Obj->TryGetNumberField(TEXT("durabilityMax"),   Entry.durabilityMax);
        Result.repairedItems.Add(Entry);
    }
    return Result;
}
