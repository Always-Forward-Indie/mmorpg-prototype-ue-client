#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"
#include "UObject/Interface.h"
#include "IPlayerProgression.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPlayerProgression : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for objects that can receive and display player progression updates
 * Follows Interface Segregation Principle by focusing only on progression functionality
 */
class PROTOTYPING_API IPlayerProgression
{
    GENERATED_BODY()

public:
    // Core progression methods
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Player Progression")
    void OnExperienceGained(const FExperienceGainEventStruct& ExperienceEvent);
    virtual void OnExperienceGained_Implementation(const FExperienceGainEventStruct& ExperienceEvent) {}

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Player Progression")
    void OnLevelUp(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience);
    virtual void OnLevelUp_Implementation(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience) {}

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Player Progression")
    void OnProgressionUpdated(const FPlayerProgressionStruct& NewProgression);
    virtual void OnProgressionUpdated_Implementation(const FPlayerProgressionStruct& NewProgression) {}

    // Query methods
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Player Progression")
    int32 GetCurrentLevel() const;
    virtual int32 GetCurrentLevel_Implementation() const { return 1; }

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Player Progression")
    int32 GetCurrentExperience() const;
    virtual int32 GetCurrentExperience_Implementation() const { return 0; }

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Player Progression")
    float GetExperienceToNextLevelPercent() const;
    virtual float GetExperienceToNextLevelPercent_Implementation() const { return 0.0f; }
};