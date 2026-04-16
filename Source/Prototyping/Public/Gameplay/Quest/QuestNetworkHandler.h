#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "QuestNetworkHandler.generated.h"

// Forward declarations
class UQuestManager;
class UNetworkManager;

/**
 * QuestNetworkHandler
 *
 * Subscribes to OnChunkServerDataReceived and routes:
 *   QUEST_UPDATE     ? QuestManager::OnQuestUpdated
 *   quest_offered    ? QuestManager::OnQuestOffered
 *   quest_turned_in  ? QuestManager::OnQuestTurnedIn
 *   exp_received     ? QuestManager::OnExpReceived
 *   item_received    ? QuestManager::OnItemReceived
 *   gold_received    ? QuestManager::OnGoldReceived
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UQuestNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UQuestNetworkHandler();

    UFUNCTION(BlueprintCallable, Category = "Quest Network")
    void Initialize(UQuestManager* InQuestManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Quest Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Quest Network")
    void UnsubscribeFromNetworkEvents();

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    // Parsers
    FQuestProgressData      ParseQuestUpdate(const TSharedPtr<FJsonObject>& Body)      const;
    FQuestOfferedData       ParseQuestOffered(const TSharedPtr<FJsonObject>& Body)     const;
    FQuestTurnedInData      ParseQuestTurnedIn(const TSharedPtr<FJsonObject>& Body)    const;

    // Helper: parse a rewards JSON array into TArray<FQuestRewardData>
    TArray<FQuestRewardData>    ParseRewards(const TArray<TSharedPtr<FJsonValue>>& Arr) const;

    // Helper: parse a currentStepEnriched / currentStep JSON object
    FQuestStepEnrichedData      ParseEnrichedStep(const TSharedPtr<FJsonObject>& Obj)  const;

    // Helper: extract kill/collect progress integers from raw JSON objects
    void ExtractStepProgress(const TSharedPtr<FJsonObject>& ProgressObj,
                             const TSharedPtr<FJsonObject>& RequiredObj,
                             const FString& StepType,
                             int32& OutCurrent,
                             int32& OutRequired) const;

    UPROPERTY()
    UQuestManager* QuestManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
