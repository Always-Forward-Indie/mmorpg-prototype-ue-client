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

USkillSlotWidget::USkillSlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SkillManager = nullptr;
    SlotIndex = -1;
    AssignedSkillSlug = "";
    bIsOnCooldown = false;
    bIsEnabled = true;
    bIsHighlighted = false;
    bIsDropHighlighted = false;
    bMousePressed = false;
    bIsDragging = false;
    CooldownRemainingTime = 0.0f;
    CooldownTotalTime = 0.0f;
}

void USkillSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Ensure this UserWidget itself is Visible so it participates in hit-testing.
    // This is what allows Slate to route DragOver/Drop events to us.
    SetVisibility(ESlateVisibility::Visible);

    if (SkillButton)
    {
        // Button handles clicks via our NativeOnMouseButtonDown/Up — we keep it
        // HitTestInvisible so it does NOT intercept the mouse hit-test chain.
        // This is critical: UE5 routes NativeOnDrop to the widget that "owns" the
        // last successful NativeOnDragOver. If SkillButton is Visible/Enabled it
        // steals the hit, UserWidget never gets DragOver, and NativeOnDrop never
        // fires. With HitTestInvisible the button is skipped for hit-test but the
        // UserWidget (us) becomes the receiver for all drag events.
        SkillButton->SetVisibility(ESlateVisibility::HitTestInvisible);

        // Still wire the UButton delegates — they fire via our custom
        // NativeOnMouseButtonDown/Up override, not via the button's own hit.
        SkillButton->OnClicked.AddDynamic(this, &USkillSlotWidget::OnSkillButtonClicked);
        SkillButton->OnPressed.AddDynamic(this, &USkillSlotWidget::OnSkillButtonPressed);
        SkillButton->OnReleased.AddDynamic(this, &USkillSlotWidget::OnSkillButtonReleased);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SkillSlotWidget[%d]: SkillButton is NULL - events not bound!"), SlotIndex);
    }

    // All decoration widgets (icons, overlays, borders, text) must be
    // SelfHitTestInvisible — they render but pass mouse/drag events up to us.
    auto MakePassThrough = [](UWidget* W)
    {
        if (W && W->GetVisibility() != ESlateVisibility::Collapsed)
        {
            W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
    };
    MakePassThrough(SkillIcon);
    MakePassThrough(CooldownOverlay);
    MakePassThrough(CooldownProgress);
    MakePassThrough(CooldownText);
    MakePassThrough(HotkeyText);
    MakePassThrough(HotkeyBackground);
    MakePassThrough(HighlightBorder);
    MakePassThrough(DropHighlightBorder);

    UpdateVisualState();
}

void USkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Stuck-drag watchdog: if bIsDragging has been true for > 5 seconds,
    // force-reset it — something went wrong with the Slate event chain.
    if (bIsDragging)
    {
        const float Now = GetCurrentTime();
        if (StuckDragStartTime <= 0.0f)
        {
            StuckDragStartTime = Now;
        }
        else if (Now - StuckDragStartTime > 5.0f)
        {
            UE_LOG(LogTemp, Error, TEXT("SkillSlotWidget[%d]: STUCK DRAG DETECTED — bIsDragging=true for %.1fs, force-resetting"),
                SlotIndex, Now - StuckDragStartTime);
            ForceResetDragState();
            StuckDragStartTime = 0.0f;
        }
    }
    else
    {
        StuckDragStartTime = 0.0f;
    }

    // Update cooldown if skill manager is available
    if (SkillManager && !AssignedSkillSlug.IsEmpty())
    {
        bool bWasOnCooldown = bIsOnCooldown;

        // Skill is on cooldown if either per-skill CD or GCD is active
        const bool bSkillCD = SkillManager->IsSkillOnCooldown(AssignedSkillSlug);
        const bool bGCD     = SkillManager->IsGCDActive();
        bIsOnCooldown = bSkillCD || bGCD;
        
        if (bIsOnCooldown)
        {
            // Show the longer of skill CD remaining vs GCD remaining
            const float SkillCDRemaining = SkillManager->GetSkillCooldownRemaining(AssignedSkillSlug);
            const float GCDRemaining     = SkillManager->GetGCDRemaining();
            CooldownRemainingTime = FMath::Max(SkillCDRemaining, GCDRemaining);

            // Use the matching total time for progress bar
            if (SkillCDRemaining >= GCDRemaining)
            {
                // Per-skill cooldown dominates
                CooldownTotalTime = CurrentSkillData.networkData.cooldownMs / 1000.0f;
            }
            else
            {
                // GCD dominates — use GCD duration as total (reasonable approximation)
                CooldownTotalTime = FMath::Max(CooldownTotalTime, GCDRemaining + 0.01f);
            }
            
            UpdateCooldownDisplay();

            // First tick entering cooldown state: show the overlay.
            // UpdateVisualState() controls CooldownOverlay visibility and icon tint.
            // Without this call the overlay stays Hidden (set on construct/clear)
            // and the progress bar + text inside it are never visible.
            if (!bWasOnCooldown)
            {
                UpdateVisualState();
            }
        }
        else if (bWasOnCooldown)
        {
            // Cooldown just finished
            CooldownRemainingTime = 0.0f;
            UpdateCooldownDisplay();
            UpdateVisualState();
        }
    }
}

FReply USkillSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: MouseDown — bIsDragging=%d bMousePressed=%d slug='%s'"),
            SlotIndex, bIsDragging ? 1 : 0, bMousePressed ? 1 : 0, *AssignedSkillSlug);

        // Safety reset: if bIsDragging is stuck from a prior drag that never
        // received a matching NativeOnMouseButtonUp or NativeOnDragCancelled,
        // force-reset it here so the slot remains usable.
        if (bIsDragging)
        {
            UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: NativeOnMouseButtonDown found stuck bIsDragging=true — force-resetting"),
                SlotIndex);
            ForceResetDragState();
        }

        bMousePressed = true;
        OnSkillButtonPressed();

        // If a skill is assigned, allow initiating a drag to remove it
        if (!AssignedSkillSlug.IsEmpty())
        {
            UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: MouseDown — returning DetectDrag for '%s'"), SlotIndex, *AssignedSkillSlug);
            return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
        }

        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: MouseDown — empty slot, no drag"), SlotIndex);
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
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: MouseUp — bIsDragging=%d bMousePressed=%d"),
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
        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: NativeOnMouseButtonUp reset bIsDragging=false"), SlotIndex);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}


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

bool USkillSlotWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: Drop RECEIVED — not a SkillDragDropOperation, rejecting"), SlotIndex);
        ResetDragVisualState();
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: Drop RECEIVED sourceSlot=%d slug='%s'"),
        SlotIndex, SkillOp->SourceSlotIndex, *SkillOp->SkillData.networkData.skillSlug);

    // Validate we have an actual skill to assign.
    const FString& DroppedSlug = SkillOp->SkillData.networkData.skillSlug;
    if (DroppedSlug.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: Drop rejected — empty skill slug"), SlotIndex);
        ResetDragVisualState();
        return false;
    }

    // If SkillManager is present, do a fast sanity check.
    // But even without it we forward the event — SkillBarWidget owns the
    // authoritative logic and will validate + reject if needed.
    if (SkillManager && !SkillManager->HasSkill(DroppedSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: Drop rejected — skill '%s' not known to SkillManager"),
            SlotIndex, *DroppedSlug);
        ResetDragVisualState();
        return false;
    }

    // If the skill was dragged from another slot, clear the source slot
    if (SkillOp->SourceSlotIndex >= 0 && SkillOp->SourceSlotWidget && SkillOp->SourceSlotIndex != SlotIndex)
    {
        SkillOp->SourceSlotWidget->OnSkillSlotDragCleared.Broadcast(SkillOp->SourceSlotIndex);
        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Clearing source slot %d after drop"),
            SlotIndex, SkillOp->SourceSlotIndex);
    }

    const FPlayerSkillData DroppedSkill = SkillOp->SkillData;
    const FKey Hotkey = GetSlotHotkey();

    ResetDragVisualState();

    OnSkillDroppedOnSlot.Broadcast(SlotIndex, DroppedSkill, Hotkey);

    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Dropped skill '%s'"),
        SlotIndex, *DroppedSlug);

    return true;
}

void USkillSlotWidget::NativeOnDragLeave(const FDragDropEvent& E, UDragDropOperation* Op)
{
    Super::NativeOnDragLeave(E, Op);

    UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: DragLeave op=%p highlight=%d"),
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
        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Drag cancelled — clearing slot"), SlotIndex);
        OnSkillSlotDragCleared.Broadcast(SlotIndex);
    }

    bIsDragging = false;
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: NativeOnDragCancelled — bIsDragging=false bMousePressed=%d"),
        SlotIndex, bMousePressed ? 1 : 0);
}

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


void USkillSlotWidget::SetOwnerBar(USkillBarWidget* InOwnerBar)
{
    OwnerBar = InOwnerBar;
}

void USkillSlotWidget::SlotInitialize(int32 InSlotIndex, UPlayerSkillManager* InSkillManager)
{
    SlotIndex = InSlotIndex;
    SkillManager = InSkillManager;

    if (SkillManager)
    {
        // Subscribe to cooldown events for more efficient updates
        SkillManager->OnSkillCooldownStarted.AddDynamic(this, &USkillSlotWidget::OnSkillCooldownStarted);
        SkillManager->OnSkillReady.AddDynamic(this, &USkillSlotWidget::OnSkillReady);

        // Get current slot data
        FSkillSlotData SlotData = SkillManager->GetSkillSlot(SlotIndex);
        SetSlotData(SlotData);

        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget: Initialized slot %d with event subscriptions"), SlotIndex);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget: Initialized slot %d without SkillManager"), SlotIndex);
    }
}

void USkillSlotWidget::SetSkillData(const FPlayerSkillData& SkillData)
{
    CurrentSkillData = SkillData;
    AssignedSkillSlug = SkillData.networkData.skillSlug;

    if (!AssignedSkillSlug.IsEmpty())
    {
        // Get cooldown info
        if (SkillManager)
        {
            CooldownTotalTime = SkillData.networkData.cooldownMs / 1000.0f; // Convert ms to seconds
            bIsOnCooldown = SkillManager->IsSkillOnCooldown(AssignedSkillSlug);
            if (bIsOnCooldown)
            {
                CooldownRemainingTime = SkillManager->GetSkillCooldownRemaining(AssignedSkillSlug);
            }
            UE_LOG(LogTemp, Warning,
                TEXT("SkillSlotWidget[%d]: SetSkillData slug='%s' cooldownMs=%d total=%.1f onCD=%d remaining=%.1f"),
                SlotIndex, *AssignedSkillSlug,
                SkillData.networkData.cooldownMs, CooldownTotalTime,
                bIsOnCooldown ? 1 : 0, CooldownRemainingTime);
        }
    }

    UpdateVisualState();
    UpdateCooldownDisplay();
}

void USkillSlotWidget::SetSlotData(const FSkillSlotData& SlotData)
{
    CurrentSlotData = SlotData;

    if (SlotData.bIsAssigned && !SlotData.skillSlug.IsEmpty())
    {
        // Get skill data if available
        if (SkillManager && SkillManager->HasSkill(SlotData.skillSlug))
        {
            FPlayerSkillData SkillData = SkillManager->GetSkillData(SlotData.skillSlug);
            SetSkillData(SkillData);
        }
        else
        {
            AssignedSkillSlug = SlotData.skillSlug;
        }
    }
    else
    {
        ClearSlot();
    }

    UpdateHotkeyDisplay();
}

void USkillSlotWidget::ClearSlot()
{
    AssignedSkillSlug = "";
    CurrentSkillData = FPlayerSkillData();
    CurrentSlotData = FSkillSlotData();
    bIsOnCooldown = false;
    CooldownRemainingTime = 0.0f;
    CooldownTotalTime = 0.0f;

    UpdateVisualState();
    UpdateCooldownDisplay();
    UpdateHotkeyDisplay();
}

void USkillSlotWidget::UpdateCooldown(float RemainingTime, float TotalTime)
{
    CooldownRemainingTime = RemainingTime;
    CooldownTotalTime = TotalTime;
    bIsOnCooldown = RemainingTime > 0.0f;

    UpdateCooldownDisplay();
    UpdateVisualState();
}

void USkillSlotWidget::SetEnabled(bool bEnabled)
{
    bIsEnabled = bEnabled;
    
    if (SkillButton)
    {
        SkillButton->SetIsEnabled(bEnabled);
    }

    UpdateVisualState();
}

void USkillSlotWidget::SetHighlighted(bool bHighlighted)
{
    bIsHighlighted = bHighlighted;
    UpdateVisualState();
}

void USkillSlotWidget::SetDropHighlighted(bool bDropHighlighted)
{
    if (bIsDropHighlighted == bDropHighlighted)
    {
        return;
    }
    
    bIsDropHighlighted = bDropHighlighted;
    UpdateVisualState();
}

void USkillSlotWidget::OnSkillButtonClicked()
{
    if (bIsEnabled && !AssignedSkillSlug.IsEmpty())
    {
        OnSkillSlotClicked.Broadcast(SlotIndex, AssignedSkillSlug);
        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget: Clicked slot %d with skill %s"), SlotIndex, *AssignedSkillSlug);
    }
}

void USkillSlotWidget::OnSkillButtonPressed()
{
    // Visual feedback for button press
    if (SkillButton)
    {
        // Could add pressed visual state here
    }
}

void USkillSlotWidget::OnSkillButtonReleased()
{
    // Visual feedback for button release
    if (SkillButton)
    {
        // Could add released visual state here
    }
}

void USkillSlotWidget::UpdateVisualState()
{
    if (!SkillIcon)
    {
        return;
    }

    // Update skill icon
    if (!AssignedSkillSlug.IsEmpty() && CurrentSkillData.definitionData.skillIcon.IsValid())
    {
        if (UTexture2D* IconTexture = CurrentSkillData.definitionData.skillIcon.LoadSynchronous())
        {
            SkillIcon->SetBrushFromTexture(IconTexture);
        }
        else if (DefaultSkillIcon)
        {
            SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
        }
    }
    else if (DefaultSkillIcon)
    {
        SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
    }

    // Update color tint based on state
    FLinearColor ColorTint = EnabledColor;
    
    if (!bIsEnabled)
    {
        ColorTint = DisabledColor;
    }
    else if (bIsOnCooldown)
    {
        ColorTint = CooldownColor;
    }

    SkillIcon->SetColorAndOpacity(ColorTint);

    // Update highlight border
    if (HighlightBorder)
    {
        HighlightBorder->SetVisibility(bIsHighlighted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
        if (bIsHighlighted)
        {
            HighlightBorder->SetColorAndOpacity(HighlightColor);
        }
    }

    // Update drop highlight border
    if (DropHighlightBorder)
    {
        ESlateVisibility NewVisibility = bIsDropHighlighted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
        DropHighlightBorder->SetVisibility(NewVisibility);
        
        if (bIsDropHighlighted)
        {
            DropHighlightBorder->SetColorAndOpacity(DropHighlightColor);
        }
    }

    // Update cooldown overlay
    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(bIsOnCooldown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
    }
}

void USkillSlotWidget::UpdateCooldownDisplay()
{
    if (CooldownProgress)
    {
        if (bIsOnCooldown && CooldownTotalTime > 0.0f)
        {
            float Progress = 1.0f - (CooldownRemainingTime / CooldownTotalTime);
            CooldownProgress->SetPercent(Progress);
            CooldownProgress->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            CooldownProgress->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    if (CooldownText)
    {
        if (bIsOnCooldown && CooldownRemainingTime > 0.0f)
        {
            FString CooldownString = FormatCooldownTime(CooldownRemainingTime);
            CooldownText->SetText(FText::FromString(CooldownString));
            CooldownText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            CooldownText->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void USkillSlotWidget::UpdateHotkeyDisplay()
{
    if (!HotkeyText)
    {
        return;
    }

    if (CurrentSlotData.boundKey.IsValid())
    {
        // Получаем читаемое имя клавиши
        FString HotkeyString = CurrentSlotData.boundKey.GetDisplayName().ToString();

        // Подменяем слова на цифры
        static const TMap<FString, FString> Replacement = {
            {TEXT("One"), TEXT("1")},
            {TEXT("Two"), TEXT("2")},
            {TEXT("Three"), TEXT("3")},
            {TEXT("Four"), TEXT("4")},
            {TEXT("Five"), TEXT("5")},
            {TEXT("Six"), TEXT("6")},
            {TEXT("Seven"), TEXT("7")},
            {TEXT("Eight"), TEXT("8")},
            {TEXT("Nine"), TEXT("9")},
            {TEXT("Zero"), TEXT("0")}
        };

        if (const FString* ReplacementValue = Replacement.Find(HotkeyString))
        {
            HotkeyString = *ReplacementValue;
        }

        HotkeyText->SetText(FText::FromString(HotkeyString));
        HotkeyText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (HotkeyBackground) {
            HotkeyBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
    }
    else
    {
        HotkeyText->SetVisibility(ESlateVisibility::Hidden);
        if (HotkeyBackground) {
            HotkeyBackground->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

FString USkillSlotWidget::FormatCooldownTime(float Time) const
{
    if (Time < 1.0f)
    {
        return FString::Printf(TEXT("%.1f"), Time);
    }
    else if (Time < 60.0f)
    {
        return FString::Printf(TEXT("%.0f"), Time);
    }
    else
    {
        int32 Minutes = static_cast<int32>(Time / 60.0f);
        int32 Seconds = static_cast<int32>(Time) % 60;
        return FString::Printf(TEXT("%d:%02d"), Minutes, Seconds);
    }
}

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

void USkillSlotWidget::BeginDestroy()
{
    // Unsubscribe from events before destruction.
    // IsValid() guards against GC'd / pending-kill SkillManager objects that a
    // bare pointer check would pass — calling RemoveDynamic on a destroyed
    // UObject corrupts the delegate and can crash during engine teardown.
    if (IsValid(SkillManager))
    {
        SkillManager->OnSkillCooldownStarted.RemoveDynamic(this, &USkillSlotWidget::OnSkillCooldownStarted);
        SkillManager->OnSkillReady.RemoveDynamic(this, &USkillSlotWidget::OnSkillReady);
    }

    Super::BeginDestroy();
}

void USkillSlotWidget::OnSkillCooldownStarted(const FString& SkillSlug)
{
    UE_LOG(LogTemp, Warning,
        TEXT("SkillSlotWidget[%d]: OnSkillCooldownStarted '%s' assigned='%s' match=%d"),
        SlotIndex, *SkillSlug, *AssignedSkillSlug,
        (SkillSlug == AssignedSkillSlug) ? 1 : 0);
    if (SkillSlug == AssignedSkillSlug && SkillManager)
    {
        CooldownTotalTime = CurrentSkillData.networkData.cooldownMs / 1000.0f;
        CooldownRemainingTime = SkillManager->GetSkillCooldownRemaining(SkillSlug);
        bIsOnCooldown = (CooldownRemainingTime > 0.0f);

        UE_LOG(LogTemp, Warning,
            TEXT("SkillSlotWidget[%d]: applying cooldown total=%.1f remaining=%.1f onCD=%d"),
            SlotIndex, CooldownTotalTime, CooldownRemainingTime, bIsOnCooldown ? 1 : 0);

        UpdateCooldownDisplay();
        UpdateVisualState();
    }
}

void USkillSlotWidget::OnSkillReady(const FString& SkillSlug)
{
    // Only update if this slot contains the skill that became ready
    if (SkillSlug == AssignedSkillSlug)
    {
        bIsOnCooldown = false;
        CooldownRemainingTime = 0.0f;
        
        UpdateCooldownDisplay();
        UpdateVisualState();
        
        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Skill %s is ready"), SlotIndex, *SkillSlug);
    }
}