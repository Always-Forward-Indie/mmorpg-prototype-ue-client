#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Gameplay/Player/IPlayerProgression.h"
#include "Engine/DataTable.h"
#include "ExperienceManager.generated.h"

// Forward declarations
class UMyGameInstance;
class UNetworkManager;
class UTimeSyncService;

// Delegates for experience system events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExperienceGained, const FExperienceGainEventStruct&, ExperienceEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLevelUp, int32, OldLevel, int32, NewLevel, int32, NewTotalExperience);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProgressionUpdated, const FPlayerProgressionStruct&, NewProgression);

/**
 * Professional Experience Manager following SOLID principles
 * 
 * Single Responsibility: Manages only player experience and level progression
 * Open/Closed: Extensible through delegates without modification
 * Liskov Substitution: Can be substituted with derived classes
 * Interface Segregation: Uses focused IPlayerProgression interface
 * Dependency Inversion: Depends on abstractions (interfaces), not concrete classes
 * 
 * Note: All level calculations and requirements are handled server-side.
 * This manager only processes and stores data received from the server.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UExperienceManager : public UObject
{
    GENERATED_BODY()

public:
    UExperienceManager();

    // Initialization and lifecycle
    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    void Initialize(UMyGameInstance* GameInstance, UNetworkManager* NetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    void Shutdown();

    // Core progression management
    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    void ProcessExperienceUpdate(const FExperienceUpdateStruct& ExperienceUpdate);

    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    void UpdateCharacterProgression(int32 CharacterId, const FPlayerProgressionStruct& NewProgression);

    // Query methods
    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    FPlayerProgressionStruct GetCharacterProgression(int32 CharacterId) const;

    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    bool HasCharacterProgression(int32 CharacterId) const;

    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    float GetExperienceProgressToNextLevel(int32 CharacterId) const;

    // Progression listener management
    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    void RegisterProgressionListener(const TScriptInterface<IPlayerProgression>& Listener);

    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    void UnregisterProgressionListener(const TScriptInterface<IPlayerProgression>& Listener);

    // Experience reason utility
    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    static EExperienceReason ParseExperienceReason(const FString& ReasonString);

    UFUNCTION(BlueprintCallable, Category = "Experience Manager")
    static FString ExperienceReasonToString(EExperienceReason Reason);

    // Event delegates
    UPROPERTY(BlueprintAssignable, Category = "Experience Manager|Events")
    FOnExperienceGained OnExperienceGained;

    UPROPERTY(BlueprintAssignable, Category = "Experience Manager|Events")
    FOnLevelUp OnLevelUp;

    UPROPERTY(BlueprintAssignable, Category = "Experience Manager|Events")
    FOnProgressionUpdated OnProgressionUpdated;

protected:
    // Internal progression processing
    void ProcessLevelUp(int32 CharacterId, int32 OldLevel, int32 NewLevel, int32 NewTotalExperience);
    
    void NotifyProgressionListeners(const FPlayerProgressionStruct& Progression, bool bLeveledUp, int32 OldLevel = 0);
    
    void NotifyExperienceGain(const FExperienceGainEventStruct& ExperienceEvent);

    // Data validation
    bool ValidateExperienceUpdate(const FExperienceUpdateStruct& ExperienceUpdate) const;
    
    bool ValidateCharacterProgression(const FPlayerProgressionStruct& Progression) const;

    // Cleanup utilities
    void CleanupInvalidListeners();

private:
    // References to core systems
    UPROPERTY()
    TObjectPtr<UMyGameInstance> GameInstance;

    UPROPERTY()
    TObjectPtr<UNetworkManager> NetworkManager;

    // Character progression storage
    UPROPERTY()
    TMap<int32, FPlayerProgressionStruct> CharacterProgressions;

    // Progression listeners using Interface
    UPROPERTY()
    TArray<TScriptInterface<IPlayerProgression>> ProgressionListeners;

    // Settings
    UPROPERTY()
    int32 MaxLevel = 100;

    UPROPERTY()
    bool bDebugLogging = true;

    // Helper methods
    void LogExperienceEvent(const FString& Event, const FString& Details) const;
};