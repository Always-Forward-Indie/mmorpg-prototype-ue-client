#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "MasteryNetworkHandler.generated.h"

class UMasteryManager;
class UNetworkManager;
class UMyGameInstance;

/**
 * Subscribes to the chunk-server feed and routes mastery packets
 * to UMasteryManager.
 *
 * Handled events:
 *   player_masteries   — full snapshot on login
 *   mastery_update     — delta after every 10 hits or tier crossing
 */
UCLASS()
class PROTOTYPING_API UMasteryNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UMasteryManager* InManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    UFUNCTION(BlueprintCallable, Category = "Mastery Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Mastery Network")
    void UnsubscribeFromNetworkEvents();

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    FPlayerMasteriesState ParseMasteriesState(const TSharedPtr<FJsonObject>& Body) const;
    FMasteryUpdateData    ParseMasteryUpdate (const TSharedPtr<FJsonObject>& Body) const;

    UPROPERTY()
    UMasteryManager* Manager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    int32 LocalCharacterId = 0;
    bool  bIsSubscribed    = false;
};
