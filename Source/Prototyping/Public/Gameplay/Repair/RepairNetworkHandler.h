#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
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
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet to filter).
    void HandleChunkPacket(const FParsedPacket& Packet);

    // EventTypes owned by this handler (single list drives dispatcher registration).
    static const TArray<FString>& HandledEvents();

    FRepairShopData       ParseRepairShop(const TSharedPtr<FJsonObject>& Body) const;
    FRepairItemResultData  ParseRepairItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const;
    FRepairAllResultData   ParseRepairAllResult(const TSharedPtr<FJsonObject>& Body, const FString& Message) const;

    UPROPERTY()
    URepairManager* RepairManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
