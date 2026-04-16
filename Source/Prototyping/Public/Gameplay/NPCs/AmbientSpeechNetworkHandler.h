#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
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
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

private:
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
