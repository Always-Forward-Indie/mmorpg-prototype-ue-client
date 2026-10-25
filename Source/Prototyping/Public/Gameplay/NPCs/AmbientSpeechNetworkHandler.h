#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "AmbientSpeechNetworkHandler.generated.h"

class UAmbientSpeechManager;
class UNetworkManager;

/**
 * AmbientSpeechNetworkHandler
 *
 * Subscribes to the chunk server delegate and handles the "NPC_AMBIENT_POOLS"
 * event. Parses each NPC entry into FAmbientSpeechNPCData and routes it to
 * UAmbientSpeechManager.
 *
 * Owned by UMyGameInstance.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UAmbientSpeechNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Bind the handler to its manager and network manager.
     * Must be called before SubscribeToNetworkEvents().
     */
    void Initialize(UAmbientSpeechManager* InManager, UNetworkManager* InNetworkManager);

    /** Subscribe to UNetworkManager::OnChunkServerDataReceived. */
    UFUNCTION(BlueprintCallable, Category = "Ambient Speech Network")
    void SubscribeToNetworkEvents();

    /** Unsubscribe (e.g. on shutdown). */
    UFUNCTION(BlueprintCallable, Category = "Ambient Speech Network")
    void UnsubscribeFromNetworkEvents();

protected:
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet to filter).
    void HandleChunkPacket(const FParsedPacket& Packet);

private:
    // EventTypes owned by this handler (single list drives dispatcher registration).
    static const TArray<FString>& HandledEvents();

    void HandleAmbientPools(const FString& JsonData);

    /** Parse one pool object {"priority":0,"lines":[...]} */
    FAmbientSpeechPoolData ParsePool(const TSharedPtr<FJsonObject>& Obj) const;

    /** Parse one line object */
    FAmbientSpeechLineData ParseLine(const TSharedPtr<FJsonObject>& Obj) const;

    UPROPERTY()
    UAmbientSpeechManager* AmbientSpeechManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
