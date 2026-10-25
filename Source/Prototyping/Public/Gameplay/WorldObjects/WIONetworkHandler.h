#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WIODataStructs.h"
#include "Networking/NetworkDispatcher.h"
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
	// Packet entry (S05): single-parse path via the per-eventType dispatcher.
	void HandleChunkPacket(const FParsedPacket& Packet);

	// EventTypes owned by this handler (single list drives dispatcher registration).
	static const TArray<FString>& HandledEvents();

	UPROPERTY()
	UWorldObjectManager* WorldObjectManager = nullptr;

	UPROPERTY()
	UNetworkManager* NetworkManager = nullptr;

	bool bIsSubscribed = false;
};
