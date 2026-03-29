#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "RepairManager.generated.h"

class UNetworkManager;
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepairShopOpened,     const FRepairShopData&,      ShopData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepairItemResult,     const FRepairItemResultData&,Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepairAllResult,      const FRepairAllResultData&, Result);

/**
 * RepairManager
 *
 * Sends openRepairShop / repairItem / repairAll requests and exposes the current
 * repair shop list via delegates.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API URepairManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    // --- Outgoing requests ---

    UFUNCTION(BlueprintCallable, Category = "Repair")
    void RequestOpenRepairShop(int32 CharacterId, int32 NpcId, const FVector& PlayerPosition);

    UFUNCTION(BlueprintCallable, Category = "Repair")
    void RequestRepairItem(int32 CharacterId, int32 NpcId, int32 InventoryItemId, const FVector& PlayerPosition);

    UFUNCTION(BlueprintCallable, Category = "Repair")
    void RequestRepairAll(int32 CharacterId, int32 NpcId, const FVector& PlayerPosition);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Repair")
    FRepairShopData GetCurrentShop() const { return CurrentShop; }

    // --- Called by RepairNetworkHandler ---

    void OnRepairShopReceived(const FRepairShopData& ShopData);
    void OnRepairItemResultReceived(const FRepairItemResultData& Result);
    void OnRepairAllResultReceived(const FRepairAllResultData& Result);

    // --- Events ---

    UPROPERTY(BlueprintAssignable, Category = "Repair Events")
    FOnRepairShopOpened OnRepairShopOpenedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Repair Events")
    FOnRepairItemResult OnRepairItemResultDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Repair Events")
    FOnRepairAllResult OnRepairAllResultDelegate;

private:
    FRepairShopData CurrentShop;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    void SendPacket(const FString& JsonPayload);
};
