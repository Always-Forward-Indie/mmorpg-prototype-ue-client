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

void UQuestJournalWidget::RefreshQuestList()
{
    if (!QuestManager || !Quest_List_Box)
    {
        return;
    }

    Quest_List_Box->ClearChildren();
    RowBindings.Reset();
    TrackBindings.Reset();

    TArray<FQuestProgressData> Quests = QuestManager->GetAllQuests();
    ULocalizationSubsystem* LocSys = GetLocSys();

    for (const FQuestProgressData& Quest : Quests)
    {
        UUserWidget* Row = nullptr;

        if (QuestRowClass)
        {
            Row = CreateWidget<UUserWidget>(GetOwningPlayer(), QuestRowClass);
        }

        if (Row)
        {
            UTextBlock* NameText = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Quest_Row_Name")));
            if (NameText)
            {
                FText DisplayText;
                if (LocSys && !Quest.clientQuestKey.IsEmpty())
                {
                    DisplayText = LocSys->GetQuestDisplayName(Quest.clientQuestKey);
                }
                else
                {
                    const FString FallbackKey = Quest.clientQuestKey.IsEmpty()
                        ? FString::Printf(TEXT("Quest #%d"), Quest.questId)
                        : Quest.clientQuestKey;
                    DisplayText = FText::FromString(FallbackKey);
                }
                NameText->SetText(DisplayText);

                FLinearColor StateColor = FLinearColor::White;
                if (Quest.state == TEXT("completed"))  StateColor = FLinearColor(0.2f, 1.f, 0.2f, 1.f);
                if (Quest.state == TEXT("turned_in"))  StateColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.f);
                if (Quest.state == TEXT("failed"))     StateColor = FLinearColor(1.f, 0.2f, 0.2f, 1.f);
                NameText->SetColorAndOpacity(FSlateColor(StateColor));
            }

            UButton* RowBtn = Cast<UButton>(Row->GetWidgetFromName(TEXT("Quest_Row_Button")));
            if (RowBtn)
            {
                UQuestRowBinding* Binding = NewObject<UQuestRowBinding>(this);
                Binding->Setup(this, Quest.questId);
                RowBindings.Add(Binding);
                RowBtn->OnClicked.AddDynamic(Binding, &UQuestRowBinding::HandleClicked);
            }

            // Optional track toggle button — only active quests get one
            UButton* TrackBtn = Cast<UButton>(Row->GetWidgetFromName(TEXT("Quest_Row_Track")));
            if (TrackBtn)
            {
                if (Quest.state == TEXT("active"))
                {
                    UQuestTrackBinding* TrackBinding = NewObject<UQuestTrackBinding>(this);
                    TrackBinding->Setup(this, Quest.questId);
                    TrackBindings.Add(TrackBinding);
                    TrackBtn->OnClicked.AddDynamic(TrackBinding, &UQuestTrackBinding::HandleTrackToggled);
                    TrackBtn->SetIsEnabled(true);
                    TrackBtn->SetVisibility(ESlateVisibility::Visible);

                    UTextBlock* TrackLabel = Cast<UTextBlock>(TrackBtn->GetChildAt(0));
                    if (TrackLabel)
                    {
                        TrackLabel->SetText(FText::FromString(Quest.bTracked ? TEXT("Tracking") : TEXT("Track")));
                    }
                }
                else
                {
                    TrackBtn->SetIsEnabled(false);
                    TrackBtn->SetVisibility(ESlateVisibility::Collapsed);
                }
            }

            Quest_List_Box->AddChild(Row);
        }
        else
        {
            // Fallback: plain button with quest key as label
            UButton* Btn = NewObject<UButton>(this);
            if (Btn)
            {
                UQuestRowBinding* Binding = NewObject<UQuestRowBinding>(this);
                Binding->Setup(this, Quest.questId);
                RowBindings.Add(Binding);
                Btn->OnClicked.AddDynamic(Binding, &UQuestRowBinding::HandleClicked);
                Quest_List_Box->AddChild(Btn);
            }
        }
    }
}

void UQuestJournalWidget::DispatchQuestRowClicked(int32 QuestId)
{
    ShowQuestDetail(QuestId);
}

void UQuestJournalWidget::DispatchQuestTrackToggled(int32 QuestId)
{
    if (!QuestManager) return;
    FQuestProgressData Data = QuestManager->GetQuestData(QuestId);
    QuestManager->SetQuestTracked(QuestId, !Data.bTracked);
}

