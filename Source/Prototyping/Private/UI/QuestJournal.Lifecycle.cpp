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

void UQuestJournalWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
    {
        Close_Button->OnClicked.AddDynamic(this, &UQuestJournalWidget::HandleCloseButtonClicked);
    }

    // Position window in the center of the viewport initially
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        const float InitScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
        const FVector2D VPSizeInit = FVector2D(W, H) / InitScale;
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D(
            FMath::Max(0.f, (VPSizeInit.X - Size.X) * 0.5f),
            FMath::Max(0.f, (VPSizeInit.Y - Size.Y) * 0.5f));
        SetPositionInViewport(CurrentViewportPosition, false);
    }

    SetVisibility(ESlateVisibility::Collapsed);

    if (ULocalizationSubsystem* LocSys = GetLocSys())
    {
        LocSys->OnLocaleChanged.AddDynamic(this, &UQuestJournalWidget::HandleLocaleChanged);
    }
}

void UQuestJournalWidget::NativeDestruct()
{
    if (ULocalizationSubsystem* LocSys = GetLocSys())
    {
        LocSys->OnLocaleChanged.RemoveDynamic(this, &UQuestJournalWidget::HandleLocaleChanged);
    }
    Super::NativeDestruct();
}

void UQuestJournalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
}

void UQuestJournalWidget::BindToQuestManager(UQuestManager* InQuestManager)
{
    if (!InQuestManager)
    {
        return;
    }

    // Unbind from old manager
    if (QuestManager)
    {
        QuestManager->OnQuestUpdatedDelegate.RemoveDynamic(this, &UQuestJournalWidget::HandleQuestUpdated);
        QuestManager->OnQuestOfferedDelegate.RemoveDynamic(this, &UQuestJournalWidget::HandleQuestOffered);
        QuestManager->OnQuestTurnedInDelegate.RemoveDynamic(this, &UQuestJournalWidget::HandleQuestTurnedIn);
    }

    QuestManager = InQuestManager;

    QuestManager->OnQuestUpdatedDelegate.AddDynamic(this, &UQuestJournalWidget::HandleQuestUpdated);
    QuestManager->OnQuestOfferedDelegate.AddDynamic(this, &UQuestJournalWidget::HandleQuestOffered);
    QuestManager->OnQuestTurnedInDelegate.AddDynamic(this, &UQuestJournalWidget::HandleQuestTurnedIn);

    RefreshQuestList();
}

