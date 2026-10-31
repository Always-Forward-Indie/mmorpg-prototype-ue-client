#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Networking/NetworkDispatcher.h"
#include "TeleportNetworkHandler.generated.h"

class UNetworkManager;
class UTeleportManager;

/**
 * Chunk-server packet entry for teleport-class events.
 *
 * Owns respawnResult + playerTeleported (dispatcher-registered, same
 * discipline as UVendorNetworkHandler). Parses via FTeleportParser (pure,
 * spec-pinned) and forwards to UTeleportManager, which runs the cover
 * transition. UPlayerManager keeps only the respawnRequest sender.
 */
UCLASS()
class PROTOTYPING_API UTeleportNetworkHandler : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UTeleportManager* InTeleportManager, UNetworkManager* InNetworkManager);

	UFUNCTION(BlueprintCallable, Category = "Teleport Network")
	void SubscribeToNetworkEvents();

	UFUNCTION(BlueprintCallable, Category = "Teleport Network")
	void UnsubscribeFromNetworkEvents();

private:
	// Packet entry (S05): single-parse path via the per-eventType dispatcher.
	void HandleChunkPacket(const FParsedPacket& Packet);

	// EventTypes owned by this handler (single list drives dispatcher registration).
	static const TArray<FString>& HandledEvents();

	void HandleRespawnResult(const FParsedPacket& Packet);
	void HandlePlayerTeleported(const FParsedPacket& Packet);

	UPROPERTY()
	UTeleportManager* TeleportManager = nullptr;

	UPROPERTY()
	UNetworkManager* NetworkManager = nullptr;

	bool bIsSubscribed = false;
};
