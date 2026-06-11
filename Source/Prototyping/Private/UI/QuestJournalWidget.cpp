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

void UQuestJournalWidget::RefreshQuestList()
{
    if (!QuestManager || !Quest_List_Box)
    {
        return;
    }

    Quest_List_Box->ClearChildren();
    RowBindings.Reset();

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

FReply UQuestJournalWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bool bShouldStartDrag = false;

        if (DragHandle)
        {
            const FGeometry DragHandleGeometry = DragHandle->GetCachedGeometry();
            const FVector2D LocalMousePos = DragHandleGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D DragHandleSize = DragHandleGeometry.GetLocalSize();
            bShouldStartDrag = (LocalMousePos.X >= 0 && LocalMousePos.X <= DragHandleSize.X &&
                                LocalMousePos.Y >= 0 && LocalMousePos.Y <= DragHandleSize.Y);
        }
        else
        {
            bShouldStartDrag = true;
        }

        if (bShouldStartDrag)
        {
            const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
            const FVector2D MouseVP = InMouseEvent.GetScreenSpacePosition() / Scale;
            DragOffset = MouseVP - CurrentViewportPosition;
            bDragging = true;

            if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            {
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            }
            return FReply::Handled();
        }
    }
    return FReply::Unhandled();
}

FReply UQuestJournalWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        if (TSharedPtr<SWidget> Slate = GetCachedWidget())
        {
            return FReply::Handled().ReleaseMouseCapture();
        }
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply UQuestJournalWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

void UQuestJournalWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);

    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D ViewportSize = FVector2D(W, H) / Scale;

    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(600, 400);

    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, FMath::Max(0.f, ViewportSize.X - Size.X));
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - Size.Y));

    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}


