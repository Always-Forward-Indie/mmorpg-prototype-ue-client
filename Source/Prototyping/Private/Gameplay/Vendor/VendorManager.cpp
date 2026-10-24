#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UVendorManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InNetworkManager || !InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("VendorManager: Initialize called with null parameters"));
        return;
    }
    NetworkManager = InNetworkManager;
    GameInstance   = InGameInstance;
}

// ---------------------------------------------------------------------------
// Outgoing requests
// ---------------------------------------------------------------------------

void UVendorManager::RequestOpenVendorShop(int32 CharacterId, int32 NpcId, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Pos    = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("openVendorShop"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Pos->SetNumberField(TEXT("x"), PlayerPosition.X);
    Pos->SetNumberField(TEXT("y"), PlayerPosition.Y);
    Pos->SetNumberField(TEXT("z"), PlayerPosition.Z);

    Body->SetNumberField(TEXT("characterId"),    CharacterId);
    Body->SetNumberField(TEXT("npcId"),          NpcId);
    Body->SetObjectField(TEXT("playerPosition"), Pos);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    SendPacket(Payload);
}

void UVendorManager::RequestBuyItem(int32 CharacterId, int32 NpcId, int32 ItemId, int32 Quantity, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Pos    = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("buyItem"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Pos->SetNumberField(TEXT("x"), PlayerPosition.X);
    Pos->SetNumberField(TEXT("y"), PlayerPosition.Y);
    Pos->SetNumberField(TEXT("z"), PlayerPosition.Z);

    Body->SetNumberField(TEXT("characterId"),    CharacterId);
    Body->SetNumberField(TEXT("npcId"),          NpcId);
    Body->SetNumberField(TEXT("itemId"),         ItemId);
    Body->SetNumberField(TEXT("quantity"),       Quantity);
    Body->SetObjectField(TEXT("playerPosition"), Pos);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    SendPacket(Payload);
}

void UVendorManager::RequestSellItem(int32 CharacterId, int32 NpcId, int32 InventoryItemId, int32 Quantity, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Pos    = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("sellItem"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Pos->SetNumberField(TEXT("x"), PlayerPosition.X);
    Pos->SetNumberField(TEXT("y"), PlayerPosition.Y);
    Pos->SetNumberField(TEXT("z"), PlayerPosition.Z);

    Body->SetNumberField(TEXT("characterId"),     CharacterId);
    Body->SetNumberField(TEXT("npcId"),           NpcId);
    Body->SetNumberField(TEXT("inventoryItemId"), InventoryItemId);
    Body->SetNumberField(TEXT("quantity"),        Quantity);
    Body->SetObjectField(TEXT("playerPosition"),  Pos);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);


    SendPacket(Payload);
}

void UVendorManager::RequestBuyItemBatch(int32 CharacterId, int32 NpcId,
    const TArray<FVendorCartEntry>& Cart, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance || Cart.IsEmpty()) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Pos    = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("buyItemBatch"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Pos->SetNumberField(TEXT("x"), PlayerPosition.X);
    Pos->SetNumberField(TEXT("y"), PlayerPosition.Y);
    Pos->SetNumberField(TEXT("z"), PlayerPosition.Z);

    TArray<TSharedPtr<FJsonValue>> ItemsArray;
    for (const FVendorCartEntry& Entry : Cart)
    {
        TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
        ItemObj->SetNumberField(TEXT("itemId"),  Entry.itemId);
        ItemObj->SetNumberField(TEXT("quantity"), Entry.quantity);
        ItemsArray.Add(MakeShared<FJsonValueObject>(ItemObj));
    }

    Body->SetNumberField(TEXT("characterId"),    CharacterId);
    Body->SetNumberField(TEXT("npcId"),          NpcId);
    Body->SetArrayField (TEXT("items"),          ItemsArray);
    Body->SetObjectField(TEXT("playerPosition"), Pos);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    SendPacket(Payload);
}

void UVendorManager::RequestSellItemBatch(int32 CharacterId, int32 NpcId,
    const TArray<FVendorCartEntry>& Cart, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance || Cart.IsEmpty()) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Pos    = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("sellItemBatch"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Pos->SetNumberField(TEXT("x"), PlayerPosition.X);
    Pos->SetNumberField(TEXT("y"), PlayerPosition.Y);
    Pos->SetNumberField(TEXT("z"), PlayerPosition.Z);

    TArray<TSharedPtr<FJsonValue>> ItemsArray;
    for (const FVendorCartEntry& Entry : Cart)
    {
        TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
        ItemObj->SetNumberField(TEXT("inventoryItemId"), Entry.inventoryItemId);
        ItemObj->SetNumberField(TEXT("quantity"),         Entry.quantity);
        ItemsArray.Add(MakeShared<FJsonValueObject>(ItemObj));
    }

    Body->SetNumberField(TEXT("characterId"),    CharacterId);
    Body->SetNumberField(TEXT("npcId"),          NpcId);
    Body->SetArrayField (TEXT("items"),          ItemsArray);
    Body->SetObjectField(TEXT("playerPosition"), Pos);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    SendPacket(Payload);
}

// ---------------------------------------------------------------------------
// Incoming � called by VendorNetworkHandler
// ---------------------------------------------------------------------------

void UVendorManager::OnVendorShopReceived(const FVendorShopData& ShopData)
{
    FVendorShopData Resolved = ShopData;
    ResolveShopStatics(Resolved);
    CurrentShop = Resolved;
    OnVendorShopOpenedDelegate.Broadcast(Resolved);
}

void UVendorManager::ResolveShopStatics(FVendorShopData& Shop)
{
    if (!GameInstance) return;
    UItemManager* ItemMgr = GameInstance->GetItemManager();
    if (!ItemMgr) return;
    for (FVendorShopItemData& Entry : Shop.items)
    {
        // S2.4 slim rows carry dynamics (+slug) only; statics come from the
        // versioned itemCatalog. Full rows (old servers) already have them.
        if (Entry.itemId <= 0) continue;
        if (!Entry.itemTypeSlug.IsEmpty() && !Entry.raritySlug.IsEmpty()) continue;
        FInventoryItemStruct Stats;
        if (!ItemMgr->GetCatalogStats(Entry.itemId, Stats)) continue;
        Entry.slug = Stats.slug;
        Entry.itemSlug = Stats.slug;
        Entry.itemType = Stats.item_type_id;
        Entry.itemTypeSlug = Stats.itemTypeSlug;
        Entry.rarityId = Stats.rarityId;
        Entry.raritySlug = Stats.raritySlug;
        Entry.stackMax = Stats.stackSize;
        Entry.weight = Stats.weight;
        Entry.equipSlotSlug = Stats.equipSlotSlug;
        Entry.levelRequirement = Stats.levelRequirement;
        Entry.setSlug = Stats.setSlug;
        Entry.masterySlug = Stats.masterySlug;
        Entry.isDurable = Stats.isDurable;
        Entry.isTradable = Stats.isTradable;
        Entry.isEquippable = Stats.isEquippable;
        Entry.isUsable = Stats.isUsable;
        Entry.isTwoHanded = Stats.isTwoHanded;
        Entry.isQuestItem = Stats.isQuestItem;
        Entry.isContainer = Stats.isContainer;
        Entry.useEffects = Stats.useEffects;
        Entry.attributes = Stats.attributes;
        // itemName/description stay empty: UI resolves via Loc/humanized slug.
    }
}

void UVendorManager::OnBuyItemResultReceived(const FBuyItemResultData& Result)
{
    OnBuyItemResultDelegate.Broadcast(Result);
}

void UVendorManager::OnSellItemResultReceived(const FSellItemResultData& Result)
{
    OnSellItemResultDelegate.Broadcast(Result);
}

void UVendorManager::OnBuyItemBatchResultReceived(const FBuyItemBatchResultData& Result)
{
    OnBuyItemBatchResultDelegate.Broadcast(Result);
}

void UVendorManager::OnSellItemBatchResultReceived(const FSellItemBatchResultData& Result)
{
    OnSellItemBatchResultDelegate.Broadcast(Result);
}

void UVendorManager::SendPacket(const FString& JsonPayload)
{
    if (NetworkManager)
    {
        NetworkManager->SendDataToChunkServer(JsonPayload);
    }
}
