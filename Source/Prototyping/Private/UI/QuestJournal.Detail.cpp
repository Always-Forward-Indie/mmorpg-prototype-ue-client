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

void UQuestJournalWidget::ShowQuestDetail(int32 QuestId)
{
    if (!QuestManager)
    {
        return;
    }

    SelectedQuestId = QuestId;
    FQuestProgressData Data = QuestManager->GetQuestData(QuestId);
    ULocalizationSubsystem* LocSys = GetLocSys();

    if (Quest_Title_Text)
    {
        FText TitleText;
        if (LocSys && !Data.clientQuestKey.IsEmpty())
        {
            TitleText = LocSys->GetQuestDisplayName(Data.clientQuestKey);
        }
        else
        {
            const FString Key = Data.clientQuestKey.IsEmpty()
                ? FString::Printf(TEXT("Quest #%d"), QuestId)
                : Data.clientQuestKey;
            TitleText = FText::FromString(Key);
        }
        Quest_Title_Text->SetText(TitleText);
    }

    if (Quest_State_Text)
    {
        Quest_State_Text->SetText(FText::FromString(Data.state.ToUpper()));
    }

    if (Quest_Step_Text)
    {
        FText StepText;
        if (LocSys && !Data.clientStepKey.IsEmpty())
        {
            StepText = LocSys->GetQuestStepDescription(Data.clientStepKey);
        }
        else
        {
            const FString StepKey = Data.clientStepKey.IsEmpty()
                ? FString::Printf(TEXT("Step %d (%s)"), Data.stepIndex, *Data.stepType)
                : Data.clientStepKey;
            StepText = FText::FromString(StepKey);
        }
        Quest_Step_Text->SetText(StepText);
    }

    if (Quest_Progress_Text)
    {
        Quest_Progress_Text->SetText(FText::FromString(FormatStepProgress(Data)));
    }

    if (Quest_Hint_Text)
    {
        FText HintText;
        if (LocSys && !Data.clientStepKey.IsEmpty())
        {
            HintText = LocSys->GetQuestStepHint(Data.clientStepKey);
        }
        Quest_Hint_Text->SetText(HintText);
        Quest_Hint_Text->SetVisibility(HintText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }

    if (Quest_Description_Text)
    {
        FText DescText;
        if (LocSys && !Data.clientQuestKey.IsEmpty())
        {
            DescText = LocSys->GetQuestDescription(Data.clientQuestKey);
        }
        Quest_Description_Text->SetText(DescText);
    }

    if (Quest_Target_Text)
    {
        const FString TargetStr = BuildTargetText(Data);
        Quest_Target_Text->SetText(FText::FromString(TargetStr));
        Quest_Target_Text->SetVisibility(
            TargetStr.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }

    if (Quest_Rewards_Box && RewardRowClass)
    {
        if (Data.rewards.Num() > 0)
        {
            PopulateRewardBox(Quest_Rewards_Box, Data.rewards);
            if (Quest_Rewards_Text) Quest_Rewards_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            Quest_Rewards_Box->ClearChildren();
            Quest_Rewards_Box->SetVisibility(ESlateVisibility::Collapsed);
            if (Quest_Rewards_Text) Quest_Rewards_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else if (Quest_Rewards_Text)
    {
        if (Data.rewards.Num() > 0)
        {
            const FString RewardStr = FString(TEXT("Rewards: ")) + BuildRewardText(Data.rewards);
            Quest_Rewards_Text->SetText(FText::FromString(RewardStr));
            Quest_Rewards_Text->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            Quest_Rewards_Text->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UQuestJournalWidget::ToggleJournal()
{
    const bool bVisible = GetVisibility() == ESlateVisibility::Visible;

    if (bVisible)
    {
        SetVisibility(ESlateVisibility::Collapsed);
        OnQuestJournalVisibilityChanged.Broadcast(false);
    }
    else
    {
        RefreshQuestList();
        if (SelectedQuestId > 0 && QuestManager && QuestManager->HasQuest(SelectedQuestId))
        {
            ShowQuestDetail(SelectedQuestId);
        }
        else if (QuestManager)
        {
            const TArray<FQuestProgressData> Quests = QuestManager->GetAllQuests();
            if (Quests.Num() > 0)
            {
                ShowQuestDetail(Quests[0].questId);
            }
        }
        SetVisibility(ESlateVisibility::Visible);
        OnQuestJournalVisibilityChanged.Broadcast(true);
    }
}

