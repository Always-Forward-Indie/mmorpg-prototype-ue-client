#include "UI/QuestJournalWidget.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/GameInstance.h"

void UQuestJournalWidget::PopulateRewardBox(UVerticalBox* Box, const TArray<FQuestRewardData>& Rewards)
{
    Box->ClearChildren();
    for (const FQuestRewardData& R : Rewards)
    {
        UQuestRewardRowWidget* Row = CreateWidget<UQuestRewardRowWidget>(GetOwningPlayer(), RewardRowClass);
        if (Row)
        {
            Row->SetupFromQuestReward(R);
            Box->AddChild(Row);
        }
    }
    Box->SetVisibility(Rewards.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

FString UQuestJournalWidget::BuildRewardText(const TArray<FQuestRewardData>& Rewards) const
{
    ULocalizationSubsystem* LocSys = GetLocSys();
    TArray<FString> Parts;
    for (const FQuestRewardData& R : Rewards)
    {
        if (R.rewardType == TEXT("exp"))
        {
            Parts.Add(FString::Printf(TEXT("* %d EXP"), R.amount));
        }
        else if (R.rewardType == TEXT("gold"))
        {
            Parts.Add(FString::Printf(TEXT("* %d Gold"), R.amount));
        }
        else if (R.rewardType == TEXT("item"))
        {
            if (R.isHidden)
            {
                Parts.Add(TEXT("* ???"));
            }
            else
            {
                FString ItemName;
                if (LocSys && !R.itemSlug.IsEmpty())
                    ItemName = LocSys->GetItemDisplayName(R.itemSlug).ToString();
                else
                    ItemName = R.itemSlug;
                Parts.Add(FString::Printf(TEXT("* %dx %s"), R.quantity, *ItemName));
            }
        }
    }
    FString Result;
    for (int32 i = 0; i < Parts.Num(); ++i)
    {
        if (i > 0) Result += TEXT("  ");
        Result += Parts[i];
    }
    return Result;
}

FString UQuestJournalWidget::BuildTargetText(const FQuestProgressData& Data) const
{
    if (!Data.bHasEnrichedStep) return TEXT("");

    ULocalizationSubsystem* LocSys = GetLocSys();
    const FQuestStepEnrichedData& Step = Data.currentStepEnriched;

    if (Step.stepType == TEXT("kill"))
    {
        FString Target = (LocSys && !Step.targetSlug.IsEmpty())
            ? LocSys->GetMobDisplayName(Step.targetSlug).ToString()
            : Step.targetSlug;
        return FString::Printf(TEXT("Kill: %s  %d/%d"), *Target, Step.current, Step.count);
    }
    else if (Step.stepType == TEXT("collect"))
    {
        FString Target = (LocSys && !Step.targetSlug.IsEmpty())
            ? LocSys->GetItemDisplayName(Step.targetSlug).ToString()
            : Step.targetSlug;
        return FString::Printf(TEXT("Collect: %s  %d/%d"), *Target, Step.current, Step.count);
    }
    else if (Step.stepType == TEXT("talk"))
    {
        FString Target = (LocSys && !Step.targetSlug.IsEmpty())
            ? LocSys->GetNPCDisplayName(Step.targetSlug).ToString()
            : Step.targetSlug;
        return FString::Printf(TEXT("Talk to: %s"), *Target);
    }
    else if (Step.stepType == TEXT("reach"))
    {
        FString Zone = (LocSys && !Step.zoneSlug.IsEmpty())
            ? LocSys->GetZoneDisplayName(Step.zoneSlug).ToString()
            : Step.zoneSlug;
        return FString::Printf(TEXT("Go to: %s"), *Zone);
    }

    return Step.clientStepKey;
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

