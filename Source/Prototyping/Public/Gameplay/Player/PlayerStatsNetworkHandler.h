#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "PlayerStatsNetworkHandler.generated.h"

class UPlayerStatsManager;
class UNetworkManager;
class UMyGameInstance;

// Fired when an effectTick packet arrives (broadcast by server for any DoT/HoT tick)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEffectTick, const FEffectTickData&, TickData);

/**
 * Subscribes to the chunk-server feed, picks up "stats_update" packets
 * and forwards parsed data to UPlayerStatsManager.
 * Only processes packets whose characterId matches the local player.
 */
UCLASS()
class PROTOTYPING_API UPlayerStatsNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UPlayerStatsManager* InStatsManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Player Stats Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Player Stats Network")
    void UnsubscribeFromNetworkEvents();

    /** Reset so the next stats_update re-triggers NotifyStatsReceived (call on level transition). */
    void ResetFirstStatsFlag() { bFirstStatsDelivered = false; }

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

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    bool bIsSubscribed = false;

    // True after the first stats_update for our local character has been delivered.
    // Used to signal the loading screen gate exactly once per session.
    bool bFirstStatsDelivered = false;
};
