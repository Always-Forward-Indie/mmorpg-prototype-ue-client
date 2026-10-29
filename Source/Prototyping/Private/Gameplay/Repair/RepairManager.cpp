#include "Gameplay/Repair/RepairManager.h"
#include "Prototyping.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Server-confirmed repair durability flows through the single durability
// channel (UEquipmentManager::ApplyDurabilityUpdate): it patches the
// equipment slot and the inventory row and re-broadcasts both states.
// Repair always restores to full, so max == current here; the dedicated
// durabilityUpdate packet carries the same values (idempotent convergence).
static void ApplyRepairDurability(UMyGameInstance* GameInstance, int32 InventoryItemId, int32 DurabilityCurrent, int32 DurabilityMax)
{
    if (!GameInstance || InventoryItemId <= 0 || DurabilityCurrent <= 0) return;
    UEquipmentManager* EquipmentManager = GameInstance->GetEquipmentManager();
    if (!EquipmentManager) return;
    EquipmentManager->ApplyDurabilityUpdate(GameInstance->GetCurrentCharacterID(), InventoryItemId, DurabilityCurrent, DurabilityMax, false);
}

void URepairManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InNetworkManager || !InGameInstance)
    {
        UE_LOG(LogGameplay, Error, TEXT("RepairManager: Initialize called with null parameters"));
        return;
    }
    NetworkManager = InNetworkManager;
    GameInstance   = InGameInstance;
}

// ---------------------------------------------------------------------------
// Outgoing requests
// ---------------------------------------------------------------------------

void URepairManager::RequestOpenRepairShop(int32 CharacterId, int32 NpcId, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Pos    = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("openRepairShop"));
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

void URepairManager::RequestRepairItem(int32 CharacterId, int32 NpcId, int32 InventoryItemId, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Pos    = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("repairItem"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Pos->SetNumberField(TEXT("x"), PlayerPosition.X);
    Pos->SetNumberField(TEXT("y"), PlayerPosition.Y);
    Pos->SetNumberField(TEXT("z"), PlayerPosition.Z);

    Body->SetNumberField(TEXT("characterId"),     CharacterId);
    Body->SetNumberField(TEXT("npcId"),           NpcId);
    Body->SetNumberField(TEXT("inventoryItemId"), InventoryItemId);
    Body->SetObjectField(TEXT("playerPosition"),  Pos);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    SendPacket(Payload);
}

void URepairManager::RequestRepairAll(int32 CharacterId, int32 NpcId, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Pos    = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("repairAll"));
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

// ---------------------------------------------------------------------------
// Incoming
// ---------------------------------------------------------------------------

void URepairManager::OnRepairShopReceived(const FRepairShopData& ShopData)
{
    CurrentShop = ShopData;
    OnRepairShopOpenedDelegate.Broadcast(ShopData);
}

void URepairManager::OnRepairItemResultReceived(const FRepairItemResultData& Result)
{
    if (Result.errorCode.IsEmpty())
    {
        ApplyRepairDurability(GameInstance, Result.inventoryItemId, Result.durabilityCurrent, Result.durabilityCurrent);
    }
    OnRepairItemResultDelegate.Broadcast(Result);
}

void URepairManager::OnRepairAllResultReceived(const FRepairAllResultData& Result)
{
    if (Result.errorCode.IsEmpty())
    {
        for (const FRepairedItemEntry& Entry : Result.repairedItems)
        {
            ApplyRepairDurability(GameInstance, Entry.inventoryItemId, Entry.durabilityCurrent, Entry.durabilityCurrent);
        }
    }
    OnRepairAllResultDelegate.Broadcast(Result);
}

void URepairManager::SendPacket(const FString& JsonPayload)
{
    if (NetworkManager)
    {
        NetworkManager->SendDataToChunkServer(JsonPayload);
    }
}
