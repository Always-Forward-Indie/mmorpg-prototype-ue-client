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

// ---------------------------------------------------------------------------
// UQuestRowBinding
// ---------------------------------------------------------------------------

void UQuestRowBinding::Setup(UQuestJournalWidget* InJournal, int32 InQuestId)
{
    Journal  = InJournal;
    QuestId  = InQuestId;
}

void UQuestRowBinding::HandleClicked()
{
    if (Journal)
    {
        Journal->DispatchQuestRowClicked(QuestId);
    }
}

// ---------------------------------------------------------------------------
// UQuestTrackBinding
// ---------------------------------------------------------------------------

void UQuestTrackBinding::Setup(UQuestJournalWidget* InJournal, int32 InQuestId)
{
    Journal = InJournal;
    QuestId = InQuestId;
}

void UQuestTrackBinding::HandleTrackToggled()
{
    if (Journal)
    {
        Journal->DispatchQuestTrackToggled(QuestId);
    }
}

