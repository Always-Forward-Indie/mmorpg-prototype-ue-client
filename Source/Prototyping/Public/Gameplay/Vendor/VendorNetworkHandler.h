#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "VendorNetworkHandler.generated.h"

class UNetworkManager;
class UVendorManager;

UCLASS()
class PROTOTYPING_API UVendorNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UVendorManager* InVendorManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Vendor Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Vendor Network")
    void UnsubscribeFromNetworkEvents();

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    FVendorShopData         ParseVendorShop(const TSharedPtr<FJsonObject>& Body) const;
    FBuyItemResultData       ParseBuyItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const;
    FSellItemResultData      ParseSellItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const;
    FBuyItemBatchResultData  ParseBuyItemBatchResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const;
    FSellItemBatchResultData ParseSellItemBatchResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const;

    UPROPERTY()
    UVendorManager* VendorManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
