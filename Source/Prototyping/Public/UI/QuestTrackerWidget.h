#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Data/DataStructs.h"
#include "QuestTrackerWidget.generated.h"

// Forward declarations
class UQuestManager;
class ULocalizationSubsystem;

/**
 * QuestTrackerWidget
 *
 * Compact HUD overlay showing all active quests: name + current step + progress.
 * Auto-updates whenever QuestManager broadcasts a change.
 *
 * Blueprint subclass must bind:
 *   Tracker_Box  ? UVerticalBox  (BindWidget)  � rows are added here
 *
 * Optional per-row class (set QuestRowClass in Blueprint defaults):
 *   The row widget must contain:
 *     Quest_Row_Name      ? UTextBlock  (GetWidgetFromName)
 *     Quest_Row_Step      ? UTextBlock  (GetWidgetFromName)
 *     Quest_Row_Progress  ? UTextBlock  (GetWidgetFromName)
 *   If QuestRowClass is nullptr, simple TextBlocks are created inline.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UQuestTrackerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Bind to QuestManager and subscribe to updates
    UFUNCTION(BlueprintCallable, Category = "Quest Tracker")
    void BindToQuestManager(UQuestManager* InQuestManager);

    // Force a full rebuild of the tracker list
    UFUNCTION(BlueprintCallable, Category = "Quest Tracker")
    void RefreshTracker();

    // Optional class for per-row sub-widget (set in Blueprint)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Tracker")
    TSubclassOf<UUserWidget> QuestRowClass;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleQuestUpdated(const FQuestProgressData& Data);

    UFUNCTION()
    void HandleQuestOffered(const FQuestOfferedData& Data);

    UFUNCTION()
    void HandleQuestTurnedIn(const FQuestTurnedInData& Data);

    UFUNCTION()
    void HandleLocaleChanged(const FString& NewLocale);

    // UMG binding
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UVerticalBox* Tracker_Box = nullptr;

private:
    UPROPERTY()
    UQuestManager* QuestManager = nullptr;

    ULocalizationSubsystem* GetLocSys() const;

    // Build a single text line for one quest
    FString BuildTrackerLine(const FQuestProgressData& Data) const;
};
