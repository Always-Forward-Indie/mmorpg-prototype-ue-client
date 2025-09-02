#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "ExperienceNetworkHandler.generated.h"

// Forward declarations
class UExperienceManager;
class UMyGameInstance;
class UNetworkManager;

/**
 * Network handler specifically for experience and progression related events
 * Follows Single Responsibility Principle by handling only experience network events
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UExperienceNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UExperienceNetworkHandler();

    // Initialization and lifecycle
    UFUNCTION(BlueprintCallable, Category = "Experience Network Handler")
    void Initialize(UExperienceManager* InExperienceManager, UMyGameInstance* InGameInstance, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Experience Network Handler")
    void Shutdown();

    // Network event subscription
    UFUNCTION(BlueprintCallable, Category = "Experience Network Handler")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Experience Network Handler")
    void UnsubscribeFromNetworkEvents();

    // Network event handlers
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    // Check if an event is experience-related
    UFUNCTION(BlueprintCallable, Category = "Experience Network Handler")
    bool IsExperienceEvent(const FString& EventType) const;

protected:
    // Event processing methods
    void ProcessExperienceUpdateEvent(const FString& JsonData);
    
    void ProcessLevelUpEvent(const FString& JsonData);
    
    void ProcessProgressionUpdateEvent(const FString& JsonData);

    // JSON parsing utilities
    FExperienceUpdateStruct ParseExperienceUpdateFromJson(const FString& JsonData) const;
    
    bool ValidateExperienceEventData(const FString& JsonData) const;

    // Logging utilities
    void LogNetworkEvent(const FString& Event, const FString& Details) const;

private:
    // System references
    UPROPERTY()
    TObjectPtr<UExperienceManager> ExperienceManager;

    UPROPERTY()
    TObjectPtr<UMyGameInstance> GameInstance;

    UPROPERTY()
    TObjectPtr<UNetworkManager> NetworkManager;

    // Configuration
    UPROPERTY()
    bool bDebugLogging = true;

    // Subscription state
    bool bIsSubscribed = false;
};