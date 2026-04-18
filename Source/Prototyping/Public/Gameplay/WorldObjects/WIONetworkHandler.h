#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WIODataStructs.h"
#include "WIONetworkHandler.generated.h"

// Forward declarations
class UWorldObjectManager;
class UNetworkManager;

/**
 * WIONetworkHandler
 *
 * Subscribes to OnChunkServerDataReceived and routes WIO packets to WorldObjectManager:
 *   spawnWorldObjects          → WorldObjectManager::HandleSpawnWorldObjects
 *   worldObjectInteractResult  → WorldObjectManager::HandleInteractResult
 *   worldObjectStateUpdate     → WorldObjectManager::HandleStateUpdate
 *   worldObjectChannelCancelled→ WorldObjectManager::HandleChannelCancelled
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UWIONetworkHandler : public UObject
{
	GENERATED_BODY()

public:
	UWIONetworkHandler();

	UFUNCTION(BlueprintCallable, Category = "WIO Network")
	void Initialize(UWorldObjectManager* InManager, UNetworkManager* InNetworkManager);

	UFUNCTION(BlueprintCallable, Category = "WIO Network")
	void SubscribeToNetworkEvents();

	UFUNCTION(BlueprintCallable, Category = "WIO Network")
	void UnsubscribeFromNetworkEvents();

private:
	UFUNCTION()
	void HandleChunkServerData(const FString& ReceivedData);

	UPROPERTY()
	UWorldObjectManager* WorldObjectManager = nullptr;

	UPROPERTY()
	UNetworkManager* NetworkManager = nullptr;

	bool bIsSubscribed = false;
};
