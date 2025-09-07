#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "PlayerSkillNetworkHandler.generated.h"

// Forward declarations
class UPlayerSkillManager;
class UNetworkManager;

/**
 * Handles network communication for player skills
 * Follows Single Responsibility Principle - only handles skill network events
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

    // Network event subscription
    UFUNCTION(BlueprintCallable, Category = "Player Skill Network Handler")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Player Skill Network Handler")
    void UnsubscribeFromNetworkEvents();

protected:
    // Network event handlers
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    // Skill-specific event handlers
    void HandleInitializePlayerSkills(const FString& JsonData);
    void HandleSkillCooldownUpdate(const FString& JsonData);
    void HandleSkillLevelUpdate(const FString& JsonData);

    // Dependencies
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependencies")
    TObjectPtr<UPlayerSkillManager> SkillManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dependencies")
    TObjectPtr<UNetworkManager> NetworkManager;

private:
    // Utility methods
    void LogNetworkEvent(const FString& EventType, const FString& Details);
    bool ValidateEventData(const FMessageDataStruct& MessageData, const FString& ExpectedEventType);

    bool bIsSubscribed = false;
};