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

FString UQuestJournalWidget::FormatStepProgress(const FQuestProgressData& Data) const
{
    if (Data.state == TEXT("completed") || Data.state == TEXT("turned_in"))
    {
        return TEXT("Complete");
    }
    if (Data.state == TEXT("failed"))
    {
        return TEXT("Failed");
    }

    if (Data.stepType == TEXT("kill") || Data.stepType == TEXT("collect"))
    {
        return FString::Printf(TEXT("%d / %d"), Data.progressCurrent, Data.progressRequired);
    }
    if (Data.stepType == TEXT("talk") || Data.stepType == TEXT("reach"))
    {
        return Data.progressCurrent > 0 ? TEXT("Done") : TEXT("In progress");
    }

    return Data.progressJson.IsEmpty() ? TEXT("�") : Data.progressJson;
}

// ??? Delegate handlers ????????????????????????????????????????????????????????

void UQuestJournalWidget::HandleQuestUpdated(const FQuestProgressData& Data)
{
    RefreshQuestList();
    if (SelectedQuestId == Data.questId)
    {
        ShowQuestDetail(Data.questId);
    }
}

void UQuestJournalWidget::HandleQuestOffered(const FQuestOfferedData& Data)
{
    RefreshQuestList();
}

void UQuestJournalWidget::HandleQuestTurnedIn(const FQuestTurnedInData& Data)
{
    RefreshQuestList();
    if (SelectedQuestId == Data.questId)
    {
        ShowQuestDetail(Data.questId);
    }
}

void UQuestJournalWidget::HandleCloseButtonClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnQuestJournalVisibilityChanged.Broadcast(false);
}

ULocalizationSubsystem* UQuestJournalWidget::GetLocSys() const
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return nullptr;
    return GI->GetSubsystem<ULocalizationSubsystem>();
}

