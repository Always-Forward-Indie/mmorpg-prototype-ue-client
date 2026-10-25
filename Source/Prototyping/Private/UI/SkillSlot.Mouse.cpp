#include "UI/SkillSlotWidget.h"
#include "UI/SkillSlotWidget.h"
#include "Prototyping.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/SkillDragDropOperation.h"
#include "UI/SkillBarWidget.h"
#include "Blueprint/DragDropOperation.h"

FReply USkillSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: MouseDown — bIsDragging=%d bMousePressed=%d slug='%s'"),
            SlotIndex, bIsDragging ? 1 : 0, bMousePressed ? 1 : 0, *AssignedSkillSlug);

        // Safety reset: if bIsDragging is stuck from a prior drag that never
        // received a matching NativeOnMouseButtonUp or NativeOnDragCancelled,
        // force-reset it here so the slot remains usable.
        if (bIsDragging)
        {
            UE_LOG(LogGameplay, Warning, TEXT("SkillSlotWidget[%d]: NativeOnMouseButtonDown found stuck bIsDragging=true — force-resetting"),
                SlotIndex);
            ForceResetDragState();
        }

        bMousePressed = true;
        OnSkillButtonPressed();

        // If a skill is assigned, allow initiating a drag to remove it
        if (!AssignedSkillSlug.IsEmpty())
        {
            UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: MouseDown — returning DetectDrag for '%s'"), SlotIndex, *AssignedSkillSlug);
            return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
        }

        UE_LOG(LogGameplay, Log, TEXT("SkillSlotWidget[%d]: MouseDown — empty slot, no drag"), SlotIndex);
        return FReply::Handled();
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        OnSkillSlotRightClicked.Broadcast(SlotIndex, AssignedSkillSlug);
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USkillSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: MouseUp — bIsDragging=%d bMousePressed=%d"),
            SlotIndex, bIsDragging ? 1 : 0, bMousePressed ? 1 : 0);
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bMousePressed)
    {
        bMousePressed = false;
        OnSkillButtonReleased();

        // Fire click only if we were not dragging and cursor is still within the widget bounds
        if (!bIsDragging)
        {
            const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D Size = InGeometry.GetLocalSize();
            if (LocalPos.X >= 0.f && LocalPos.X <= Size.X && LocalPos.Y >= 0.f && LocalPos.Y <= Size.Y)
            {
                OnSkillButtonClicked();
            }
        }

        bIsDragging = false;
        UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: NativeOnMouseButtonUp reset bIsDragging=false"), SlotIndex);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}


