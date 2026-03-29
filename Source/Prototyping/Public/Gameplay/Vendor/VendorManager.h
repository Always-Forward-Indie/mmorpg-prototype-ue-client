#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "VendorManager.generated.h"

class UNetworkManager;
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVendorShopOpened,         const FVendorShopData&,          ShopData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuyItemResult,             const FBuyItemResultData&,       Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSellItemResult,            const FSellItemResultData&,      Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuyItemBatchResult,        const FBuyItemBatchResultData&,  Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSellItemBatchResult,       const FSellItemBatchResultData&, Result);

/**
 * VendorManager
 *
 * Sends openVendorShop / buyItem / sellItem requests and exposes the current
 * vendor shop catalogue via delegates.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UVendorManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    // --- Outgoing requests ---

    UFUNCTION(BlueprintCallable, Category = "Vendor")
    void RequestOpenVendorShop(int32 CharacterId, int32 NpcId, const FVector& PlayerPosition);

    UFUNCTION(BlueprintCallable, Category = "Vendor")
    void RequestBuyItem(int32 CharacterId, int32 NpcId, int32 ItemId, int32 Quantity, const FVector& PlayerPosition);

    UFUNCTION(BlueprintCallable, Category = "Vendor")
    void RequestSellItem(int32 CharacterId, int32 NpcId, int32 InventoryItemId, int32 Quantity, const FVector& PlayerPosition);

    UFUNCTION(BlueprintCallable, Category = "Vendor")
    void RequestBuyItemBatch(int32 CharacterId, int32 NpcId, const TArray<FVendorCartEntry>& Cart, const FVector& PlayerPosition);

    UFUNCTION(BlueprintCallable, Category = "Vendor")
    void RequestSellItemBatch(int32 CharacterId, int32 NpcId, const TArray<FVendorCartEntry>& Cart, const FVector& PlayerPosition);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vendor")
    FVendorShopData GetCurrentShop() const { return CurrentShop; }

    // --- Called by VendorNetworkHandler ---

    void OnVendorShopReceived(const FVendorShopData& ShopData);
    void OnBuyItemResultReceived(const FBuyItemResultData& Result);
    void OnSellItemResultReceived(const FSellItemResultData& Result);
    void OnBuyItemBatchResultReceived(const FBuyItemBatchResultData& Result);
    void OnSellItemBatchResultReceived(const FSellItemBatchResultData& Result);

    // --- Events ---

    UPROPERTY(BlueprintAssignable, Category = "Vendor Events")
    FOnVendorShopOpened OnVendorShopOpenedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Vendor Events")
    FOnBuyItemResult OnBuyItemResultDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Vendor Events")
    FOnSellItemResult OnSellItemResultDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Vendor Events")
    FOnBuyItemBatchResult OnBuyItemBatchResultDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Vendor Events")
    FOnSellItemBatchResult OnSellItemBatchResultDelegate;

private:
    FVendorShopData CurrentShop;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    void SendPacket(const FString& JsonPayload);
};
