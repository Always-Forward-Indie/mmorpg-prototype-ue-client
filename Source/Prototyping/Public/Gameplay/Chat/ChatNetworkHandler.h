#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "ChatNetworkHandler.generated.h"

// Forward declarations
class UChatManager;
class UNetworkManager;

/**
 * ChatNetworkHandler
 *
 * Subscribes to OnChunkServerDataReceived and routes chatMessage packets
 * (both broadcast messages and whisper-error responses) to UChatManager.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UChatNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UChatNetworkHandler();

    UFUNCTION(BlueprintCallable, Category = "Chat Network")
    void Initialize(UChatManager* InChatManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Chat Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Chat Network")
    void UnsubscribeFromNetworkEvents();

private:
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet to filter).
    void HandleChunkPacket(const FParsedPacket& Packet);

    // EventTypes owned by this handler (single list drives dispatcher registration).
    static const TArray<FString>& HandledEvents();

    FChatMessageStruct ParseChatMessage(const TSharedPtr<FJsonObject>& Root) const;

    UPROPERTY()
    UChatManager* ChatManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
