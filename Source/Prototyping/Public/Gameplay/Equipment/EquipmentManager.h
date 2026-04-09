#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "EquipmentManager.generated.h"

class UNetworkManager;
class UMyGameInstance;
class UInventoryManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentStateChanged, const FEquipmentStateData&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipResultReceived,   const FEquipResultData&,    Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeightStatusChanged,   const FWeightStatusData&,   WeightStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttributesUpdated, int32, CharacterId, const TArray<FAttributeDataStruct>&, Attributes);
// Fired when the server sends a PLAYER_EQUIPMENT_UPDATE for any character other than the local one.
// Payload carries the full equipment state including characterId, so listeners can route it to the correct actor.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemoteEquipmentStateReceived, const FEquipmentStateData&, State);

/**
 * EquipmentManager
 *
 * Owns the client-side equipment state (slot snapshot) and weight status.
 * Sends equip/unequip/getEquipment requests to the server.
 * Handles EQUIPMENT_STATE, EQUIP_RESULT, WEIGHT_STATUS and charAttributesUpdate packets.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UEquipmentManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    // Link to InventoryManager so equip events keep is_equipped flags in sync
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void SetInventoryManager(UInventoryManager* InInventoryManager);

    // --- Outgoing requests ---

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RequestEquipItem(int32 CharacterId, int32 InventoryItemId);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RequestUnequipItem(int32 CharacterId, const FString& SlotSlug);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RequestGetEquipment(int32 CharacterId);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    FEquipmentStateData GetEquipmentState() const { return EquipmentState; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    FEquipmentSlotData GetSlot(const FString& SlotSlug) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    FWeightStatusData GetWeightStatus() const { return WeightStatus; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
    bool IsOverweight() const { return WeightStatus.isOverweight; }

    // --- Called by EquipmentNetworkHandler ---

    void OnEquipmentStateReceived(const FEquipmentStateData& State);
    void OnEquipResultReceived(const FEquipResultData& Result);
    void OnWeightStatusReceived(const FWeightStatusData& Status);
    void OnAttributesUpdated(int32 CharacterId, const TArray<FAttributeDataStruct>& Attributes);
    // Called when the server sends a PLAYER_EQUIPMENT_UPDATE for a remote character.
    void OnRemoteEquipmentStateReceived(const FEquipmentStateData& State);

    // Returns the last PLAYER_EQUIPMENT_UPDATE received for the given character, or nullptr.
    // Used by SpawnPlayerForClient to replay equipment that arrived before the actor was spawned.
    const FEquipmentStateData* GetCachedRemoteEquipmentState(int32 CharacterId) const;

    // --- Events ---

    UPROPERTY(BlueprintAssignable, Category = "Equipment Events")
    FOnEquipmentStateChanged OnEquipmentStateChangedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Equipment Events")
    FOnEquipResultReceived OnEquipResultReceivedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Equipment Events")
    FOnWeightStatusChanged OnWeightStatusChangedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Equipment Events")
    FOnAttributesUpdated OnAttributesUpdatedDelegate;

    // Fired for equipment state of other players (PLAYER_EQUIPMENT_UPDATE packet)
    UPROPERTY(BlueprintAssignable, Category = "Equipment Events")
    FOnRemoteEquipmentStateReceived OnRemoteEquipmentStateReceivedDelegate;

private:
    FEquipmentStateData EquipmentState;
    FWeightStatusData   WeightStatus;

    // Cache of the last PLAYER_EQUIPMENT_UPDATE per remote character ID.
    // Populated in OnRemoteEquipmentStateReceived; consumed in SpawnPlayerForClient
    // to handle the race where the packet arrives before the remote actor is spawned.
    TMap<int32, FEquipmentStateData> RemoteEquipmentStateCache;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    // Weak reference - EquipmentManager does NOT own InventoryManager
    UPROPERTY()
    UInventoryManager* InventoryManager = nullptr;

    void SendPacket(const FString& JsonPayload);

    // Rebuilds is_equipped on every item in InventoryManager based on current EquipmentState
    void SyncEquippedFlagsToInventory();
};
