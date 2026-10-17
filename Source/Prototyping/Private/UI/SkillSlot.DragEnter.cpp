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

void USkillSlotWidget::NativeOnDragEnter(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    Super::NativeOnDragEnter(G, E, Op);

    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp)
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: DragEnter op=%p canAccept=%d"),
        SlotIndex, Op, CanAcceptSkillDrop(SkillOp) ? 1 : 0);

    // Always accept the latest op pointer. Slate can re-enter (e.g., cursor moves
    // between child widgets of this UserWidget) producing a new Op wrapper for the
    // same logical drag — the old pointer-based guard was silently swallowing those
    // and leaving highlight in a stale state.
    if (ActiveDragOp.Get() != Op)
    {
        InvalidateDragCache();
        ActiveDragOp = Op;
    }

    // CanAcceptSkillDrop now rejects the source slot itself, so the source no
    // longer glows and steals the highlight from the real drop target.
    SetDropHighlighted(CanAcceptSkillDrop(SkillOp));

    // Clear highlight on sibling slots so only this slot glows.
    if (OwnerBar.IsValid())
    {
        OwnerBar->ClearHighlightOnOtherSlots(this);
    }
}

bool USkillSlotWidget::NativeOnDragOver(const FGeometry& Geo, const FDragDropEvent& E, UDragDropOperation* Op)
{
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp)
    {
        return false;
    }

    // Ensure op tracking is current (Enter may have been missed if cursor moved
    // in from outside the viewport or Slate re-routed the event).
    if (ActiveDragOp.Get() != Op)
    {
        InvalidateDragCache();
        ActiveDragOp = Op;
    }

    // Restore highlight if a spurious DragLeave cleared it while the cursor is
    // still over this slot.
    if (!bIsDropHighlighted)
    {
        SetDropHighlighted(CanAcceptSkillDrop(SkillOp));
    }

    // CRITICAL: always return true for a valid SkillDragDropOperation.
    // UE5 only calls NativeOnDrop if the last NativeOnDragOver returned true.
    // We gate the actual acceptance in NativeOnDrop — this just tells Slate we
    // are a candidate drop target so the event reaches us.
    return true;
}

