#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UEquipmentManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InNetworkManager || !InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("EquipmentManager: Initialize called with null parameters"));
        return;
    }
    NetworkManager = InNetworkManager;
    GameInstance   = InGameInstance;
    UE_LOG(LogTemp, Log, TEXT("EquipmentManager: Initialized"));
}

void UEquipmentManager::SetInventoryManager(UInventoryManager* InInventoryManager)
{
    InventoryManager = InInventoryManager;
}

// ---------------------------------------------------------------------------
// Outgoing requests
// ---------------------------------------------------------------------------

void UEquipmentManager::RequestEquipItem(int32 CharacterId, int32 InventoryItemId)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("equipItem"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Body->SetNumberField(TEXT("characterId"),     CharacterId);
    Body->SetNumberField(TEXT("inventoryItemId"), InventoryItemId);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    SendPacket(Payload);
    UE_LOG(LogTemp, Log, TEXT("EquipmentManager: Sent equipItem inventoryItemId=%d"), InventoryItemId);
}

void UEquipmentManager::RequestUnequipItem(int32 CharacterId, const FString& SlotSlug)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("unequipItem"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Body->SetNumberField(TEXT("characterId"),   CharacterId);
    Body->SetStringField(TEXT("equipSlotSlug"), SlotSlug);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    SendPacket(Payload);
    UE_LOG(LogTemp, Log, TEXT("EquipmentManager: Sent unequipItem slot=%s"), *SlotSlug);
}

void UEquipmentManager::RequestGetEquipment(int32 CharacterId)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("getEquipment"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Body->SetNumberField(TEXT("characterId"), CharacterId);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    SendPacket(Payload);
}

// ---------------------------------------------------------------------------
// Incoming � called by EquipmentNetworkHandler
// ---------------------------------------------------------------------------

FEquipmentSlotData UEquipmentManager::GetSlot(const FString& SlotSlug) const
{
    if (const FEquipmentSlotData* Slot = EquipmentState.slots.Find(SlotSlug))
    {
        return *Slot;
    }
    return FEquipmentSlotData{};
}

void UEquipmentManager::OnEquipmentStateReceived(const FEquipmentStateData& State)
{
    // Only apply to local equipment state if this packet is for our character.
    // If GameInstance is set and the characterId doesn't match, skip updating
    // local state (the EquipmentVisualComponent on the remote player handles it
    // via OnRemoteEquipmentStateReceivedDelegate).
    if (GameInstance && State.characterId > 0 &&
        State.characterId != GameInstance->GetCurrentCharacterID())
    {
        // Route as remote player equipment update instead
        OnRemoteEquipmentStateReceived(State);
        return;
    }

    EquipmentState = State;
    SyncEquippedFlagsToInventory();
    OnEquipmentStateChangedDelegate.Broadcast(State);
}

void UEquipmentManager::OnEquipResultReceived(const FEquipResultData& Result)
{
    // Only broadcast equip results for our own character
    if (GameInstance && Result.characterId > 0 &&
        Result.characterId != GameInstance->GetCurrentCharacterID())
    {
        return;
    }
    OnEquipResultReceivedDelegate.Broadcast(Result);

    // Re-sync so InventoryManager immediately reflects the new equip/unequip state.
    // EQUIPMENT_STATE usually follows shortly but re-syncing here avoids one-frame lag.
    SyncEquippedFlagsToInventory();
}

void UEquipmentManager::OnWeightStatusReceived(const FWeightStatusData& Status)
{
    // Only apply weight status for our own character
    if (GameInstance && Status.characterId > 0 &&
        Status.characterId != GameInstance->GetCurrentCharacterID())
    {
        return;
    }
    WeightStatus = Status;
    OnWeightStatusChangedDelegate.Broadcast(Status);
}

void UEquipmentManager::OnAttributesUpdated(int32 CharacterId, const TArray<FAttributeDataStruct>& Attributes)
{
    // Only broadcast attribute updates for our own character
    if (GameInstance && CharacterId > 0 &&
        CharacterId != GameInstance->GetCurrentCharacterID())
    {
        return;
    }
    OnAttributesUpdatedDelegate.Broadcast(CharacterId, Attributes);
}

void UEquipmentManager::OnRemoteEquipmentStateReceived(const FEquipmentStateData& State)
{
    // Cache by character ID so SpawnPlayerForClient can replay it if the actor
    // was not yet spawned when this packet arrived (race-condition on join/world load).
    if (State.characterId > 0)
    {
        RemoteEquipmentStateCache.Add(State.characterId, State);
    }
    OnRemoteEquipmentStateReceivedDelegate.Broadcast(State);
}

const FEquipmentStateData* UEquipmentManager::GetCachedRemoteEquipmentState(int32 CharacterId) const
{
    return RemoteEquipmentStateCache.Find(CharacterId);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UEquipmentManager::SyncEquippedFlagsToInventory()
{
    if (!InventoryManager) return;

    // Build a set of currently-equipped inventoryItemIds from EquipmentState
    TSet<int32> EquippedIds;
    for (const auto& SlotPair : EquipmentState.slots)
    {
        const FEquipmentSlotData& Slot = SlotPair.Value;
        if (Slot.bIsOccupied && Slot.inventoryItemId > 0)
            EquippedIds.Add(Slot.inventoryItemId);
    }

    // Patch flags directly in CurrentInventory - no full rewrite so we never
    // clobber a freshly-received inventory snapshot from the server.
    bool bAnyChanged = false;
    for (FInventoryItemStruct& Item : InventoryManager->CurrentInventory.items)
    {
        const bool bShouldBeEquipped = EquippedIds.Contains(Item.id);
        if (Item.is_equipped != bShouldBeEquipped)
        {
            Item.is_equipped = bShouldBeEquipped;
            bAnyChanged = true;
        }
    }

    if (bAnyChanged)
    {
        // Notify UI that inventory display needs a refresh
        InventoryManager->OnInventoryUpdated.Broadcast(InventoryManager->CurrentInventory);
    }
}

void UEquipmentManager::SendPacket(const FString& JsonPayload)
{
    if (NetworkManager)
    {
        NetworkManager->SendDataToChunkServer(JsonPayload);
    }
}
