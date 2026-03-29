#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "PlayerStatsNetworkHandler.generated.h"

class UPlayerStatsManager;
class UNetworkManager;

// Fired when an effectTick packet arrives (broadcast by server for any DoT/HoT tick)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEffectTick, const FEffectTickData&, TickData);

/**
 * Subscribes to the chunk-server feed, picks up "stats_update" packets
 * and forwards parsed data to UPlayerStatsManager.
 */
UCLASS()
class PROTOTYPING_API UPlayerStatsNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UPlayerStatsManager* InStatsManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Player Stats Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Player Stats Network")
    void UnsubscribeFromNetworkEvents();

    // Fired for every effectTick packet (DoT/HoT tick on any character)
    UPROPERTY(BlueprintAssignable, Category = "Player Stats Network|Events")
    FOnEffectTick OnEffectTick;

private:
UFUNCTION()
void HandleChunkServerData(const FString& ReceivedData);

FPlayerStatsUpdateStruct ParseStatsUpdate(const TSharedPtr<FJsonObject>& Body) const;
void HandleSetPlayerActiveEffects(const FString& ReceivedData) const;

    UPROPERTY()
    UPlayerStatsManager* StatsManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
