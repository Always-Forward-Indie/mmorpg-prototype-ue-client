#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
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

    // Pure packet parsers (public for Automation contract specs).
    // Message is the NORMALIZED discriminator (FResultStatusRule output):
    // "success", else an error code. Never pass raw header.message — success
    // packets carry none, and every total would parse as zero.
    // RequestIdEcho binds the result to its request (empty = predates echo).
    FVendorShopData         ParseVendorShop(const TSharedPtr<FJsonObject>& Body) const;
    FBuyItemResultData       ParseBuyItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message, const FString& RequestIdEcho = FString()) const;
    FSellItemResultData      ParseSellItemResult(const TSharedPtr<FJsonObject>& Body, const FString& Message, const FString& RequestIdEcho = FString()) const;
    FBuyItemBatchResultData  ParseBuyItemBatchResult(const TSharedPtr<FJsonObject>& Body, const FString& Message, const FString& RequestIdEcho = FString()) const;
    FSellItemBatchResultData ParseSellItemBatchResult(const TSharedPtr<FJsonObject>& Body, const FString& Message, const FString& RequestIdEcho = FString()) const;

private:
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet to filter).
    void HandleChunkPacket(const FParsedPacket& Packet);

    // EventTypes owned by this handler (single list drives dispatcher registration).
    static const TArray<FString>& HandledEvents();

    UPROPERTY()
    UVendorManager* VendorManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
