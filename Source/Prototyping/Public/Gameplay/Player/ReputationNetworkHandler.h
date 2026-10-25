#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "ReputationNetworkHandler.generated.h"

class UReputationManager;
class UNetworkManager;
class UMyGameInstance;

/**
 * Subscribes to the chunk-server feed and routes reputation packets
 * to UReputationManager.
 *
 * Handled events:
 *   player_reputations   — full snapshot on login
 *   reputation_update    — delta on every reputation change
 */
UCLASS()
class PROTOTYPING_API UReputationNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UReputationManager* InManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    UFUNCTION(BlueprintCallable, Category = "Reputation Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Reputation Network")
    void UnsubscribeFromNetworkEvents();

private:
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet twice).
    void HandleChunkPacket(const FParsedPacket& Packet);

    FPlayerReputationsState ParseReputationsState(const TSharedPtr<FJsonObject>& Body) const;
    FReputationUpdateData   ParseReputationUpdate (const TSharedPtr<FJsonObject>& Body) const;

    UPROPERTY()
    UReputationManager* Manager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    int32 LocalCharacterId = 0;
    bool  bIsSubscribed    = false;
};
