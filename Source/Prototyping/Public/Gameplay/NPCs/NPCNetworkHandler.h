#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "NPCNetworkHandler.generated.h"

// Forward declarations
class UNPCManager;
class UNetworkManager;

/**
 * Network handler specifically for NPC-related events
 * Follows Single Responsibility Principle by handling only NPC network events
 * Integrates with the existing network architecture
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UNPCNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UNPCNetworkHandler();

    // Initialization
    UFUNCTION(BlueprintCallable, Category = "NPC Network Handler")
    void Initialize(UNPCManager* InNPCManager, UNetworkManager* InNetworkManager);

    // Network event subscription
    UFUNCTION(BlueprintCallable, Category = "NPC Network Handler")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "NPC Network Handler")
    void UnsubscribeFromNetworkEvents();

    // Check if an event is NPC-related
    UFUNCTION(BlueprintCallable, Category = "NPC Network Handler")
    bool IsNPCEvent(const FString& EventType) const;

protected:
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet to filter).
    void HandleChunkPacket(const FParsedPacket& Packet);

    // NPC-specific event handlers
    void HandleNPCSpawn(const FString& JsonData);
    void HandleNPCUpdate(const FString& JsonData);
    void HandleNPCRemove(const FString& JsonData);
    void HandleNPCInteraction(const FString& JsonData);

    // EventTypes owned by this handler (single list drives dispatcher registration).
    static const TArray<FString>& HandledEvents();

    // Data validation
    bool ValidateNPCEventData(const FString& JsonData) const;

    // Logging utilities
    void LogNetworkEvent(const FString& Event, const FString& Details) const;

private:
    // System references
    UPROPERTY()
    TObjectPtr<UNPCManager> NPCManager;

    UPROPERTY()
    TObjectPtr<UNetworkManager> NetworkManager;

    // Configuration
    UPROPERTY()
    bool bDebugLogging = true;

    // Subscription state
    bool bIsSubscribed = false;
};