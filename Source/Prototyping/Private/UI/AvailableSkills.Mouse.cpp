#include "UI/AvailableSkillsWidget.h"
#include "UI/AvailableSkillsWidget.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "UI/SkillDragDropOperation.h"
#include "UI/SkillDragVisualWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Input/Events.h"

FReply UAvailableSkillsWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsVisible && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Hide tooltip when clicking anywhere in the available skills window
        HideSkillTooltip();
        
        bool bShouldStartDrag = false;

        // Debug: Log widget geometry information
        UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Mouse click - Widget Geometry: Size(%f, %f), Pos(%f, %f)"), 
            InGeometry.GetLocalSize().X, InGeometry.GetLocalSize().Y,
            InGeometry.GetAbsolutePosition().X, InGeometry.GetAbsolutePosition().Y);

        if (DragHandle)
        {
            // Check if clicking on drag handle
            const FGeometry DragHandleGeometry = DragHandle->GetCachedGeometry();
            
            // Debug: Log drag handle information
            UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: DragHandle found - Size(%f, %f), Pos(%f, %f), Visibility: %s"), 
                DragHandleGeometry.GetLocalSize().X, DragHandleGeometry.GetLocalSize().Y,
                DragHandleGeometry.GetAbsolutePosition().X, DragHandleGeometry.GetAbsolutePosition().Y,
                *UEnum::GetValueAsString(DragHandle->GetVisibility()));
            
            // Convert mouse position to local coordinates of the drag handle
            const FVector2D ScreenSpacePosition = InMouseEvent.GetScreenSpacePosition();
            const FVector2D LocalMousePos = DragHandleGeometry.AbsoluteToLocal(ScreenSpacePosition);
            const FVector2D DragHandleSize = DragHandleGeometry.GetLocalSize();
            
            // Check if mouse click is within drag handle bounds
            bShouldStartDrag = (LocalMousePos.X >= 0 && LocalMousePos.X <= DragHandleSize.X &&
                LocalMousePos.Y >= 0 && LocalMousePos.Y <= DragHandleSize.Y);
            
            UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: DragHandle click check - ScreenPos: (%f, %f), LocalPos: (%f, %f), Size: (%f, %f), ShouldDrag: %s"), 
                ScreenSpacePosition.X, ScreenSpacePosition.Y,
                LocalMousePos.X, LocalMousePos.Y, 
                DragHandleSize.X, DragHandleSize.Y, 
                bShouldStartDrag ? TEXT("true") : TEXT("false"));

            // Additional check: Is the DragHandle actually receiving input?
            if (DragHandleSize.X <= 0 || DragHandleSize.Y <= 0)
            {
                UE_LOG(LogGameplay, Error, TEXT("AvailableSkillsWidget: DragHandle has zero or negative size! This will prevent interaction."));
                
                // Fallback: Use title area for dragging (top 40 pixels of widget)
                const FVector2D LocalClickPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
                bShouldStartDrag = (LocalClickPos.Y >= 0 && LocalClickPos.Y <= 40.0f);
                
                UE_LOG(LogGameplay, Warning, TEXT("AvailableSkillsWidget: Using fallback title area drag - LocalClick: (%f, %f), ShouldDrag: %s"), 
                    LocalClickPos.X, LocalClickPos.Y, bShouldStartDrag ? TEXT("true") : TEXT("false"));
            }
        }
        else
        {
            // No specific drag handle, use title area for dragging (top 40 pixels of widget)
            const FVector2D LocalClickPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            bShouldStartDrag = (LocalClickPos.Y >= 0 && LocalClickPos.Y <= 40.0f);
            
            UE_LOG(LogGameplay, Warning, TEXT("AvailableSkillsWidget: No DragHandle found, using title area - LocalClick: (%f, %f), ShouldDrag: %s"), 
                LocalClickPos.X, LocalClickPos.Y, bShouldStartDrag ? TEXT("true") : TEXT("false"));
        }

        if (bShouldStartDrag)
        {
            const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
            const FVector2D Screen = InMouseEvent.GetScreenSpacePosition();
            const FVector2D MouseVP = Screen / Scale;
            
            DragOffset = MouseVP - CurrentViewportPosition;
            bDragging = true;

            UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Starting drag - MouseVP: (%f, %f), CurrentPos: (%f, %f), Offset: (%f, %f)"), 
                MouseVP.X, MouseVP.Y, CurrentViewportPosition.X, CurrentViewportPosition.Y, DragOffset.X, DragOffset.Y);

            if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            {
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            }
            return FReply::Handled();
        }
        else
        {
            UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Drag not started - mouse click outside drag area"));
        }
    }
    
    // Hide tooltip on right mouse button click as well
    if (bIsVisible && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        HideSkillTooltip();
    }
    
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UAvailableSkillsWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UAvailableSkillsWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UAvailableSkillsWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPosAbs)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);

    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D ViewportSize = FVector2D(W, H) / Scale;

    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400, 300);

    // Convert cursor position to viewport space
    const FVector2D MouseVP = ScreenCursorPosAbs / Scale;

    // Calculate desired position
    FVector2D Pos = MouseVP - DragOffset;

    // Clamp to viewport bounds
    Pos.X = FMath::Clamp(Pos.X, DragPadding.Left, FMath::Max(DragPadding.Left, ViewportSize.X - Size.X - DragPadding.Right));
    Pos.Y = FMath::Clamp(Pos.Y, DragPadding.Top, FMath::Max(DragPadding.Top, ViewportSize.Y - Size.Y - DragPadding.Bottom));

    // Update position
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}

