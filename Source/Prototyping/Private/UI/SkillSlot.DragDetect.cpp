#include "UI/SkillSlotWidget.h"
#include "UI/SkillSlotWidget.h"
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

void USkillSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: NativeOnDragDetected ENTERED — slug='%s' bIsDragging=%d bMousePressed=%d"),
        SlotIndex, *AssignedSkillSlug, bIsDragging ? 1 : 0, bMousePressed ? 1 : 0);

    if (AssignedSkillSlug.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: NativeOnDragDetected — slot is EMPTY, aborting"), SlotIndex);
        OutOperation = nullptr;
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Drag detected for skill '%s'"), SlotIndex, *AssignedSkillSlug);

    USkillDragDropOperation* DragDropOp = NewObject<USkillDragDropOperation>(this, USkillDragDropOperation::StaticClass());
    if (!DragDropOp)
    {
        OutOperation = nullptr;
        return;
    }

    DragDropOp->SkillData = CurrentSkillData;
    DragDropOp->SourceWidget = nullptr;
    DragDropOp->SourceSlotWidget = this;
    DragDropOp->SourceSlotIndex = SlotIndex;

    // Try to create a drag visual
    if (DragVisualWidgetClass)
    {
        DragDropOp->DragVisualWidgetClass = DragVisualWidgetClass;
    }

    UUserWidget* DragVisual = DragDropOp->CreateDragVisualWidget();
    if (DragVisual)
    {
        DragDropOp->DefaultDragVisual = DragVisual;
    }
    else
    {
        // Never use this slot as DefaultDragVisual — UMG would reparent the slot
        // out of the skill bar for the duration of the drag, corrupting the
        // layout/hit-testing of every sibling slot (root cause of the slot-to-slot
        // drag flicker and drop-on-self bug). Proceed with no visual instead.
        DragDropOp->DefaultDragVisual = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: CreateDragVisualWidget returned null — proceeding without drag visual"), SlotIndex);
    }

    DragDropOp->Pivot = EDragPivot::MouseDown;
    DragDropOp->Offset = FVector2D::ZeroVector;

    OutOperation = DragDropOp;
    bIsDragging = true;

    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Started dragging skill '%s' from slot"), SlotIndex, *AssignedSkillSlug);
}


