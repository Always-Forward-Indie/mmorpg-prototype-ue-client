#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "PlayerSkillNetworkHandler.generated.h"

// Forward declarations
class UPlayerSkillManager;
class UNetworkManager;
class UMyGameInstance;

/**
 * Handles network communication for player skills.
 * Inbound:  initializePlayerSkills, setSkillCooldowns, skillBarState, skillBarSlotUpdated
 * Outbound: setSkillBarSlot
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UPlayerSkillNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UPlayerSkillNetworkHandler();

    // Initialization
    UFUNCTION(BlueprintCallable, Category = "Player Skill Network Handler")
    void Initialize(UPlayerSkillManager* InSkillManager, UNetworkManager* InNetworkManager);

    /** Must be called after Initialize() so outbound packets can include auth data. */
    UFUNCTION(BlueprintCallable, Category = "Player Skill Network Handler")
    void SetGameInstance(UMyGameInstance* InGameInstance);

    // Network event subscription
    UFUNCTION(BlueprintCallable, Category = "Player Skill Network Handler")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Player Skill Network Handler")
    void UnsubscribeFromNetworkEvents();

    /**
     * Send setSkillBarSlot to chunk-server.
     * Call this when the local player drags a skill onto a bar slot or removes it.
     * Pass an empty SkillSlug to clear the slot.
     */
    UFUNCTION(BlueprintCallable, Category = "Player Skill Network Handler")
    void SendSetSkillBarSlot(int32 SlotIndex, const FString& SkillSlug, int32 CharacterId);

protected:
    // Packet entry (S05): single-parse path via the dispatcher fallback.
    // Replaces HandleChunkServerData (which re-parsed every packet).
    void HandleChunkPacket(const FParsedPacket& Packet);

    // Skill-specific event handlers
    void HandleInitializePlayerSkills(const FParsedPacket& Packet);
    void HandleSetSkillCooldowns(const FParsedPacket& Packet);
    void HandleSkillCooldownUpdate(const FParsedPacket& Packet);
    void HandleSkillLevelUpdate(const FParsedPacket& Packet);

    // Skill-bar event handlers
    void HandleSkillBarState(const FParsedPacket& Packet);
    void HandleSkillBarSlotUpdated(const FParsedPacket& Packet);

    // Dependencies
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependencies")
    TObjectPtr<UPlayerSkillManager> SkillManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependencies")
    TObjectPtr<UNetworkManager> NetworkManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependencies")
    TObjectPtr<UMyGameInstance> GameInstance;

private:
    // Utility methods
    void LogNetworkEvent(const FString& EventType, const FString& Details);
    bool ValidateEventData(const FMessageDataStruct& MessageData, const FString& ExpectedEventType);

    bool bIsSubscribed = false;
};