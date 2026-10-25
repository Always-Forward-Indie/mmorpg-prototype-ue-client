#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "TradeNetworkHandler.generated.h"

class UNetworkManager;
class UTradeManager;

UCLASS()
class PROTOTYPING_API UTradeNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UTradeManager* InTradeManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Trade Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Trade Network")
    void UnsubscribeFromNetworkEvents();

private:
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet twice).
    void HandleChunkPacket(const FParsedPacket& Packet);

    // EventTypes owned by this handler (single list drives dispatcher registration).
    static const TArray<FString>& HandledEvents();

    FTradeInviteData   ParseTradeInvite(const TSharedPtr<FJsonObject>& Body) const;
    FTradeStateData    ParseTradeState(const TSharedPtr<FJsonObject>& Body) const;
    FTradeDeclinedData ParseTradeDeclined(const TSharedPtr<FJsonObject>& Body) const;
    FTradeCancelledData ParseTradeCancelled(const TSharedPtr<FJsonObject>& Body) const;
    FTradeCompleteData  ParseTradeComplete(const TSharedPtr<FJsonObject>& Body) const;

    UPROPERTY()
    UTradeManager* TradeManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
