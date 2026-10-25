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

bool USkillSlotWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp)
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillSlotWidget[%d]: Drop RECEIVED — not a SkillDragDropOperation, rejecting"), SlotIndex);
        ResetDragVisualState();
        return false;
    }

    UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: Drop RECEIVED sourceSlot=%d slug='%s'"),
        SlotIndex, SkillOp->SourceSlotIndex, *SkillOp->SkillData.networkData.skillSlug);

    // Validate we have an actual skill to assign.
    const FString& DroppedSlug = SkillOp->SkillData.networkData.skillSlug;
    if (DroppedSlug.IsEmpty())
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillSlotWidget[%d]: Drop rejected — empty skill slug"), SlotIndex);
        ResetDragVisualState();
        return false;
    }

    // If SkillManager is present, do a fast sanity check.
    // But even without it we forward the event — SkillBarWidget owns the
    // authoritative logic and will validate + reject if needed.
    if (SkillManager && !SkillManager->HasSkill(DroppedSlug))
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillSlotWidget[%d]: Drop rejected — skill '%s' not known to SkillManager"),
            SlotIndex, *DroppedSlug);
        ResetDragVisualState();
        return false;
    }

    // If the skill was dragged from another slot, clear the source slot
    if (SkillOp->SourceSlotIndex >= 0 && SkillOp->SourceSlotWidget && SkillOp->SourceSlotIndex != SlotIndex)
    {
        SkillOp->SourceSlotWidget->OnSkillSlotDragCleared.Broadcast(SkillOp->SourceSlotIndex);
        UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: Clearing source slot %d after drop"),
            SlotIndex, SkillOp->SourceSlotIndex);
    }

    const FPlayerSkillData DroppedSkill = SkillOp->SkillData;
    const FKey Hotkey = GetSlotHotkey();

    ResetDragVisualState();

    OnSkillDroppedOnSlot.Broadcast(SlotIndex, DroppedSkill, Hotkey);

    UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: Dropped skill '%s'"),
        SlotIndex, *DroppedSlug);

    return true;
}

void USkillSlotWidget::NativeOnDragLeave(const FDragDropEvent& E, UDragDropOperation* Op)
{
    Super::NativeOnDragLeave(E, Op);

    UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: DragLeave op=%p highlight=%d"),
        SlotIndex, Op, bIsDropHighlighted ? 1 : 0);

    // Direct clear. The previous deferred-clear timer was a band-aid for flicker
    // caused by using the source widget itself as the drag visual (which
    // reparented the source and made Slate fire spurious 0ms DragLeave cycles on
    // sibling slots). With a standalone drag visual that root cause is gone, so
    // a straight clear is correct and avoids stuck highlights from suppressed leaves.
    if (Cast<USkillDragDropOperation>(Op))
    {
        SetDropHighlighted(false);
    }
}

void USkillSlotWidget::NativeOnDragCancelled(const FDragDropEvent& E, UDragDropOperation* Op)
{
    ResetDragVisualState();

    // If this slot initiated the drag and it was cancelled (dropped into empty space), clear the slot
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (SkillOp && SkillOp->SourceSlotIndex == SlotIndex && SkillOp->SourceSlotWidget == this)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: Drag cancelled — clearing slot"), SlotIndex);
        OnSkillSlotDragCleared.Broadcast(SlotIndex);
    }

    bIsDragging = false;
    UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget[%d]: NativeOnDragCancelled — bIsDragging=false bMousePressed=%d"),
        SlotIndex, bMousePressed ? 1 : 0);
}

