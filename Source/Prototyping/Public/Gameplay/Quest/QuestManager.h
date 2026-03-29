#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "QuestManager.generated.h"

// Forward declarations
class UNetworkManager;
class UMyGameInstance;

// Fired on any quest state/progress change
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestUpdated,      const FQuestProgressData&, QuestData);
// Fired when a quest is freshly offered
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestOffered,      const FQuestOfferedData&,  OfferedData);
// Fired when a quest is turned in
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestTurnedIn,     const FQuestTurnedInData&, TurnedInData);
// Reward notifications
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExpReceived,       const FExpReceivedData&,   ExpData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemReceived,      const FItemReceivedData&,  ItemData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldReceived,      const FGoldReceivedData&,  GoldData);

/**
 * QuestManager
 *
 * Owns the client-side quest journal (TMap<questId, FQuestProgressData>).
 * Receives updates from QuestNetworkHandler and broadcasts events to UI.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UQuestManager : public UObject
{
    GENERATED_BODY()

public:
    UQuestManager();

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    // --- Called by QuestNetworkHandler ---

    void OnQuestUpdated(const FQuestProgressData& Data);
    void OnQuestOffered(const FQuestOfferedData& Data);
    void OnQuestTurnedIn(const FQuestTurnedInData& Data);
    void OnExpReceived(const FExpReceivedData& Data);
    void OnItemReceived(const FItemReceivedData& Data);
    void OnGoldReceived(const FGoldReceivedData& Data);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    bool HasQuest(int32 QuestId) const { return QuestJournal.Contains(QuestId); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    FQuestProgressData GetQuestData(int32 QuestId) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    TArray<FQuestProgressData> GetAllQuests() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    TArray<FQuestProgressData> GetActiveQuests() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quest")
    TArray<FQuestProgressData> GetCompletedQuests() const;

    // --- Events ---

    UPROPERTY(BlueprintAssignable, Category = "Quest Events")
    FOnQuestUpdated  OnQuestUpdatedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Quest Events")
    FOnQuestOffered  OnQuestOfferedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Quest Events")
    FOnQuestTurnedIn OnQuestTurnedInDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Quest Events")
    FOnExpReceived   OnExpReceivedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Quest Events")
    FOnItemReceived  OnItemReceivedDelegate;

    UPROPERTY(BlueprintAssignable, Category = "Quest Events")
    FOnGoldReceived  OnGoldReceivedDelegate;

private:
    // questId ? latest progress snapshot
    TMap<int32, FQuestProgressData> QuestJournal;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;
};
