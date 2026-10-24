#include "UI/QuestTrackerWidget.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

// S5b: tracker fallback chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "QuestTracker"

void UQuestTrackerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);

    if (ULocalizationSubsystem* LocSys = GetLocSys())
    {
        LocSys->OnLocaleChanged.AddDynamic(this, &UQuestTrackerWidget::HandleLocaleChanged);
    }
}

void UQuestTrackerWidget::NativeDestruct()
{
    if (ULocalizationSubsystem* LocSys = GetLocSys())
    {
        LocSys->OnLocaleChanged.RemoveDynamic(this, &UQuestTrackerWidget::HandleLocaleChanged);
    }
    Super::NativeDestruct();
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

    TArray<FQuestProgressData> TrackedQuests = QuestManager->GetTrackedQuests();

    if (TrackedQuests.Num() == 0)
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);

    for (const FQuestProgressData& Quest : TrackedQuests)
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
                            ? FText::Format(LOCTEXT("QuestNumbered", "Quest #{0}"),
                                FText::AsNumber(Quest.questId)).ToString()
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
                    const FText Progress = BuildTrackerLine(Quest);
                    FText HintText;
                    if (LocSys && !Quest.clientStepKey.IsEmpty())
                    {
                        HintText = LocSys->GetQuestStepHint(Quest.clientStepKey);
                    }
                    const FText Combined = HintText.IsEmpty()
                        ? Progress
                        : (Progress.IsEmpty() ? HintText
                            : FText::Format(LOCTEXT("ProgressHint", "{0}  ({1})"), Progress, HintText));
                    ProgressText->SetText(Combined);
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
                Line->SetText(BuildTrackerLine(Quest));
                Tracker_Box->AddChild(Line);
            }
        }
    }
}

FText UQuestTrackerWidget::BuildTrackerLine(const FQuestProgressData& Data) const
{
    // Prefer enriched step data (has resolved slug names)
    if (Data.bHasEnrichedStep)
    {
        const FQuestStepEnrichedData& Step = Data.currentStepEnriched;
        ULocalizationSubsystem* LocSys = nullptr;
        if (UGameInstance* GI = GetGameInstance()) LocSys = GI->GetSubsystem<ULocalizationSubsystem>();

        if (Step.stepType == TEXT("kill"))
        {
            FText Target = (LocSys && !Step.targetSlug.IsEmpty())
                ? LocSys->GetMobDisplayName(Step.targetSlug)
                : FText::FromString(Step.targetSlug);
            return FText::Format(LOCTEXT("TargetProgress", "{0}  {1}/{2}"),
                Target, FText::AsNumber(Step.current), FText::AsNumber(Step.count));
        }
        else if (Step.stepType == TEXT("collect"))
        {
            FText Target = (LocSys && !Step.targetSlug.IsEmpty())
                ? LocSys->GetItemDisplayName(Step.targetSlug)
                : FText::FromString(Step.targetSlug);
            return FText::Format(LOCTEXT("TargetProgress", "{0}  {1}/{2}"),
                Target, FText::AsNumber(Step.current), FText::AsNumber(Step.count));
        }
        else if (Step.stepType == TEXT("talk"))
        {
            FText Target = (LocSys && !Step.targetSlug.IsEmpty())
                ? LocSys->GetNPCDisplayName(Step.targetSlug)
                : FText::FromString(Step.targetSlug);
            return FText::Format(LOCTEXT("TalkTo", "Talk to: {0}"), Target);
        }
        else if (Step.stepType == TEXT("reach"))
        {
            FText Zone = (LocSys && !Step.zoneSlug.IsEmpty())
                ? LocSys->GetZoneDisplayName(Step.zoneSlug)
                : FText::FromString(Step.zoneSlug);
            return FText::Format(LOCTEXT("GoTo", "Go to: {0}"), Zone);
        }
    }

    // Fallback: use the old flat fields
    if (Data.stepType == TEXT("kill") || Data.stepType == TEXT("collect"))
    {
        return FText::Format(LOCTEXT("ProgressFraction", "{0}/{1}"),
            FText::AsNumber(Data.progressCurrent), FText::AsNumber(Data.progressRequired));
    }
    if (Data.stepType == TEXT("talk") || Data.stepType == TEXT("reach"))
    {
        return Data.progressCurrent > 0 ? LOCTEXT("Done", "Done") : LOCTEXT("InProgress", "In progress");
    }

    return FText::GetEmpty();
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

void UQuestTrackerWidget::HandleLocaleChanged(const FString& NewLocale)
{
    RefreshTracker();
}

#undef LOCTEXT_NAMESPACE
