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

    /** Reset so the next stats_update re-triggers NotifyStatsReceived (call on level transition).
     *  Pass the new local character ID so the filter uses the correct value for the new session. */
    void ResetFirstStatsFlag(int32 NewLocalCharacterId = 0)
    {
        bFirstStatsDelivered  = false;
        FullStatsPacketCount  = 0;
        if (NewLocalCharacterId > 0)
        {
            LocalCharacterId = NewLocalCharacterId;
        }
    }

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

    // True after the loading screen gate has been signalled for this session.
    bool bFirstStatsDelivered = false;

    // Counts full stats_update packets (those that carry attributes).
    // We fire NotifyStatsReceived on the SECOND full packet so that the
    // SET_PLAYER_ACTIVE_EFFECTS stats_update (which always follows the
    // inventory-load stats_update) has time to arrive before the loading
    // screen is dismissed.  Characters with only one full packet at login
    // still trigger the gate via the BasicPlayer::ProcessStatsUpdate path.
    int32 FullStatsPacketCount = 0;

    // Character ID of the local player � cached at Initialize() time.
    // Avoids reading GameInstance ambient state on every packet callback.
    int32 LocalCharacterId = 0;
};
