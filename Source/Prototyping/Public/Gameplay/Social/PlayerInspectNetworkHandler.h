#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "PlayerInspectNetworkHandler.generated.h"

class UNetworkManager;
class UPlayerInspectManager;
struct FPlayerInspectData;

/**
 * Chunk-packet entry for the unit-menu inspect flow (Manager + Handler pair,
 * same convention as Trade). Owns `playerInspect` (+ request echo errors)
 * and forwards parsed snapshots to UPlayerInspectManager.
 */
UCLASS()
class PROTOTYPING_API UPlayerInspectNetworkHandler : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UPlayerInspectManager* InManager, UNetworkManager* InNetworkManager);

	UFUNCTION(BlueprintCallable, Category = "Inspect Network")
	void SubscribeToNetworkEvents();

	UFUNCTION(BlueprintCallable, Category = "Inspect Network")
	void UnsubscribeFromNetworkEvents();

private:
	void HandleChunkPacket(const FParsedPacket& Packet);

	static const TArray<FString>& HandledEvents();
	FPlayerInspectData ParseInspect(const TSharedPtr<FJsonObject>& Body) const;

	UPROPERTY() UPlayerInspectManager* InspectManager = nullptr;
	UPROPERTY() UNetworkManager* NetworkManager = nullptr;

	bool bIsSubscribed = false;
};
