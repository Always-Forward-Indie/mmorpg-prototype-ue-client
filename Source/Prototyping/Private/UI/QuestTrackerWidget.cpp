#include "UI/QuestTrackerWidget.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

void UQuestTrackerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UQuestTrackerWidget::BindToQuestManager(UQuestManager* InQuestManager)
{
    if (!InQuestManager)
    {
        return;
    }

    if (QuestManager)
    {
        QuestManager->OnQuestUpdatedDelegate.RemoveDynamic(this, &UQuestTrackerWidget::HandleQuestUpdated);
        QuestManager->OnQuestOfferedDelegate.RemoveDynamic(this, &UQuestTrackerWidget::HandleQuestOffered);
        QuestManager->OnQuestTurnedInDelegate.RemoveDynamic(this, &UQuestTrackerWidget::HandleQuestTurnedIn);
    }

    QuestManager = InQuestManager;

    QuestManager->OnQuestUpdatedDelegate.AddDynamic(this, &UQuestTrackerWidget::HandleQuestUpdated);
    QuestManager->OnQuestOfferedDelegate.AddDynamic(this, &UQuestTrackerWidget::HandleQuestOffered);
    QuestManager->OnQuestTurnedInDelegate.AddDynamic(this, &UQuestTrackerWidget::HandleQuestTurnedIn);

    RefreshTracker();
}

void UQuestTrackerWidget::RefreshTracker()
{
    if (!Tracker_Box || !QuestManager)
    {
        return;
    }

    Tracker_Box->ClearChildren();

    TArray<FQuestProgressData> ActiveQuests = QuestManager->GetActiveQuests();

    for (const FQuestProgressData& Quest : ActiveQuests)
    {
        if (QuestRowClass)
        {
            // Use the custom row widget defined in Blueprint
            UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), QuestRowClass);
            if (Row)
            {
                ULocalizationSubsystem* LocSys = GetLocSys();

                UTextBlock* NameText = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Quest_Row_Name")));
                if (NameText)
                {
                    FText DisplayName = (LocSys && !Quest.clientQuestKey.IsEmpty())
                        ? LocSys->GetQuestDisplayName(Quest.clientQuestKey)
                        : FText::FromString(Quest.clientQuestKey.IsEmpty()
                            ? FString::Printf(TEXT("Quest #%d"), Quest.questId)
                            : Quest.clientQuestKey);
                    NameText->SetText(DisplayName);
                }

                UTextBlock* StepText = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Quest_Row_Step")));
                if (StepText)
                {
                    FText StepDesc = (LocSys && !Quest.clientStepKey.IsEmpty())
                        ? LocSys->GetQuestStepDescription(Quest.clientStepKey)
                        : FText::FromString(Quest.clientStepKey);
                    StepText->SetText(StepDesc);
                }

                UTextBlock* ProgressText = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Quest_Row_Progress")));
                if (ProgressText)
                {
                    const FString ProgressStr = BuildTrackerLine(Quest);
                    FText HintText;
                    if (LocSys && !Quest.clientStepKey.IsEmpty())
                    {
                        HintText = LocSys->GetQuestStepHint(Quest.clientStepKey);
                    }
                    const FString HintStr = HintText.ToString();
                    const FString Combined = HintStr.IsEmpty()
                        ? ProgressStr
                        : (ProgressStr.IsEmpty() ? HintStr : FString::Printf(TEXT("%s  (%s)"), *ProgressStr, *HintStr));
                    ProgressText->SetText(FText::FromString(Combined));
                }

                Tracker_Box->AddChild(Row);
            }
        }
        else
        {
            // Fallback: plain TextBlock with all info in one line
            UTextBlock* Line = NewObject<UTextBlock>(this);
            if (Line)
            {
                Line->SetText(FText::FromString(BuildTrackerLine(Quest)));
                Tracker_Box->AddChild(Line);
            }
        }
    }
}

FString UQuestTrackerWidget::BuildTrackerLine(const FQuestProgressData& Data) const
{
    if (Data.stepType == TEXT("kill") || Data.stepType == TEXT("collect"))
    {
        return FString::Printf(TEXT("%d/%d"), Data.progressCurrent, Data.progressRequired);
    }
    if (Data.stepType == TEXT("talk") || Data.stepType == TEXT("reach"))
    {
        return Data.progressCurrent > 0 ? TEXT("Done") : TEXT("In progress");
    }

    return FString();
}

void UQuestTrackerWidget::HandleQuestUpdated(const FQuestProgressData& /*Data*/)
{
    RefreshTracker();
}

void UQuestTrackerWidget::HandleQuestOffered(const FQuestOfferedData& /*Data*/)
{
    RefreshTracker();
}

void UQuestTrackerWidget::HandleQuestTurnedIn(const FQuestTurnedInData& /*Data*/)
{
    RefreshTracker();
}

ULocalizationSubsystem* UQuestTrackerWidget::GetLocSys() const
{
    UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetSubsystem<ULocalizationSubsystem>() : nullptr;
}
