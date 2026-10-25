#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "CombatNetworkHandler.generated.h"

// Forward declarations
class UCombatSystemManager;
class UNetworkManager;

/**
 * Handles network events specifically related to the new combat system
 * Separates combat network logic from other systems
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UCombatNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UCombatNetworkHandler();

    // Initialization
    UFUNCTION(BlueprintCallable, Category = "Combat Network")
    void Initialize(UCombatSystemManager* CombatManager, UNetworkManager* NetworkManager);

    // Subscribe to network events
    UFUNCTION(BlueprintCallable, Category = "Combat Network")
    void SubscribeToNetworkManager();

    // Packet entry (S05): single-parse path via the dispatcher fallback.
    // Replaces ProcessChunkServerData (which re-parsed every packet to filter).
    void HandleChunkPacket(const FParsedPacket& Packet);

protected:
    // Handle specific combat events
    void HandleSkillInitiation(const FString& JsonData);
    void HandleSkillResult(const FString& JsonData);

private:
    UPROPERTY()
    UCombatSystemManager* CombatSystemManager;

    UPROPERTY()
    UNetworkManager* NetworkManager;

    // Logging helper
    void LogNetworkEvent(const FString& Event, const FString& Details);
};