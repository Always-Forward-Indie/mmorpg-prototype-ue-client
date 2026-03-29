#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "TradeManager.generated.h"

class UNetworkManager;
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeInviteReceived,   const FTradeInviteData&,   Invite);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeStateUpdated,     const FTradeStateData&,    State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeDeclined,         const FTradeDeclinedData&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeCancelled,        const FTradeCancelledData&,Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeCompleted,        const FTradeCompleteData&, Data);

/**
 * TradeManager
 *
 * Manages the full P2P trade lifecycle:
 *   Request ? Invite ? Accept ? Offer updates ? Confirm ? Complete
 *   Any side can Cancel at any time.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UTradeManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    // --- Outgoing requests ---

    UFUNCTION(BlueprintCallable, Category = "Trade")
    void RequestTrade(int32 CharacterId, int32 TargetCharacterId, const FVector& PlayerPosition);

    // accept = true to accept, false to decline
    UFUNCTION(BlueprintCallable, Category = "Trade")
    void RespondToTradeInvite(int32 CharacterId, bool bAccept);

    UFUNCTION(BlueprintCallable, Category = "Trade")
    void UpdateTradeOffer(int32 CharacterId, int32 Gold, const TArray<FTradeOfferItem>& Items);

    UFUNCTION(BlueprintCallable, Category = "Trade")
    void ConfirmTrade(int32 CharacterId);

    UFUNCTION(BlueprintCallable, Category = "Trade")
    void CancelTrade(int32 CharacterId);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
    FTradeStateData GetCurrentState() const { return CurrentState; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
    bool IsInTrade() const { return !CurrentSessionId.IsEmpty(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
    FTradeInviteData GetPendingInvite() const { return PendingInvite; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
    bool HasPendingInvite() const { return bHasPendingInvite; }

    // --- Called by TradeNetworkHandler ---

    void OnTradeInviteReceived(const FTradeInviteData& Invite);
    void OnTradeStateReceived(const FTradeStateData& State);
    void OnTradeDeclined(const FTradeDeclinedData& Data);
    void OnTradeCancelled(const FTradeCancelledData& Data);
    void OnTradeCompleted(const FTradeCompleteData& Data);

    // --- Events ---

    UPROPERTY(BlueprintAssignable, Category = "Trade Events")
    FOnTradeInviteReceived OnTradeInviteReceivedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Trade Events")
    FOnTradeStateUpdated OnTradeStateUpdatedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Trade Events")
    FOnTradeDeclined OnTradeDeclinedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Trade Events")
    FOnTradeCancelled OnTradeCancelledDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Trade Events")
    FOnTradeCompleted OnTradeCompletedDelegate;

private:
    FTradeStateData  CurrentState;
    FTradeInviteData PendingInvite;
    FString          CurrentSessionId;
    bool             bHasPendingInvite = false;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    void SendPacket(const FString& JsonPayload);
    void ClearSession();
};
