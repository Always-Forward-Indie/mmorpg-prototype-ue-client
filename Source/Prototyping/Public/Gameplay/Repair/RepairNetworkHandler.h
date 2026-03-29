#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "RepairNetworkHandler.generated.h"

class UNetworkManager;
class URepairManager;

UCLASS()
class PROTOTYPING_API URepairNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URepairManager* InRepairManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Repair Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Repair Network")
    void UnsubscribeFromNetworkEvents();

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    FRepairShopData       ParseRepairShop(const TSharedPtr<FJsonObject>& Body) const;
    FRepairItemResultData  ParseRepairItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const;
    FRepairAllResultData   ParseRepairAllResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const;

    UPROPERTY()
    URepairManager* RepairManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
