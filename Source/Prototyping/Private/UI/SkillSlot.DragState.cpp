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

bool USkillSlotWidget::CanAcceptSkillDrop(USkillDragDropOperation* DragDropOp) const
{
    if (!DragDropOp)
    {
        return false;
    }

    // Use cached result for the same operation pointer.
    if (bCacheValid && CachedDragOp.Get() == DragDropOp)
    {
        return bCachedCanAccept;
    }

    // Reject drop on the source slot itself. Reordering onto the origin is a
    // no-op, and previously the source stayed a valid drop candidate so it kept
    // glowing and stole the highlight from the real target — which is exactly
    // why the target slot appeared to flicker / never light up during slot-to-slot
    // drags.
    if (DragDropOp->SourceSlotWidget == this && DragDropOp->SourceSlotIndex == SlotIndex)
    {
        CachedDragOp = DragDropOp;
        bCachedCanAccept = false;
        bCacheValid = true;
        return false;
    }

    const FString& SkillSlug = DragDropOp->SkillData.networkData.skillSlug;
    if (SkillSlug.IsEmpty())
    {
        CachedDragOp = DragDropOp;
        bCachedCanAccept = false;
        bCacheValid = true;
        return false;
    }

    // If SkillManager is available, confirm the player actually has the skill.
    // Without it we optimistically accept \u2014 the authoritative check happens in
    // SkillBarWidget::OnSkillDroppedOnSlot -> AssignSkillToSlot.
    const bool bCanAccept = SkillManager ? SkillManager->HasSkill(SkillSlug) : true;

    CachedDragOp = DragDropOp;
    bCachedCanAccept = bCanAccept;
    bCacheValid = true;

    return bCanAccept;
}

FKey USkillSlotWidget::GetSlotHotkey() const
{
    if (CurrentSlotData.boundKey.IsValid())
    {
        return CurrentSlotData.boundKey;
    }

    // Return default hotkey based on slot index
    TArray<FKey> DefaultHotkeys = {
        EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
        EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero
    };

    if (SlotIndex >= 0 && SlotIndex < DefaultHotkeys.Num())
    {
        return DefaultHotkeys[SlotIndex];
    }

    return FKey();
}

void USkillSlotWidget::InvalidateDragCache()
{
    bCacheValid = false;
    CachedDragOp.Reset();
    bCachedCanAccept = false;
}

void USkillSlotWidget::ResetDragVisualState()
{
    ActiveDragOp.Reset();
    InvalidateDragCache();
    SetDropHighlighted(false);
}

float USkillSlotWidget::GetCurrentTime() const
{
    if (UWorld* World = GetWorld())
    {
        return World->GetTimeSeconds();
    }
    return 0.0f;
}

void USkillSlotWidget::ForceResetDragState()
{
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: ForceResetDragState wasDragging=%d wasMousePressed=%d wasHighlighted=%d"),
        SlotIndex, bIsDragging ? 1 : 0, bMousePressed ? 1 : 0, bIsDropHighlighted ? 1 : 0);

    ActiveDragOp.Reset();
    InvalidateDragCache();
    bIsDragging = false;
    bMousePressed = false;

    if (bIsDropHighlighted)
    {
        bIsDropHighlighted = false;
        UpdateVisualState();
    }
}

