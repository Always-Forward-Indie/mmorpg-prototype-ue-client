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

void UQuestJournalWidget::HandleLocaleChanged(const FString& NewLocale)
{
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        RefreshQuestList();
        if (SelectedQuestId > 0 && QuestManager && QuestManager->HasQuest(SelectedQuestId))
        {
            ShowQuestDetail(SelectedQuestId);
        }
    }
}
