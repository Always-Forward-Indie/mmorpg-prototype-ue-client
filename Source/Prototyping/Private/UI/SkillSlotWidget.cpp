#include "UI/SkillSlotWidget.h"
#include "UI/SkillSlotWidget.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/SkillDragDropOperation.h"
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
    CooldownRemainingTime = 0.0f;
    CooldownTotalTime = 0.0f;
    
    LastDragEnterTime = 0.0f;
    LastDragLeaveTime = 0.0f;
    LastHighlightChangeTime = 0.0f;
}

void USkillSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SkillButton)
    {
        SkillButton->OnClicked.AddDynamic(this, &USkillSlotWidget::OnSkillButtonClicked);
        SkillButton->OnPressed.AddDynamic(this, &USkillSlotWidget::OnSkillButtonPressed);
        SkillButton->OnReleased.AddDynamic(this, &USkillSlotWidget::OnSkillButtonReleased);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SkillSlotWidget[%d]: SkillButton is NULL - events not bound!"), SlotIndex);
    }

    UpdateVisualState();
}

void USkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

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
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        OnSkillSlotRightClicked.Broadcast(SlotIndex, AssignedSkillSlug);
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}


void USkillSlotWidget::NativeOnDragEnter(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    Super::NativeOnDragEnter(G, E, Op);
    
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp)
    {
        return;
    }
    
    // Guard against duplicate DragEnter for the same operation
    if (ActiveDragOp.Get() == Op)
    {
        return;
    }
    
    InvalidateDragCache();
    ActiveDragOp = Op;
    
    if (SkillButton) 
    {
        SkillButton->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    
    bool bCanAccept = CanAcceptSkillDrop(SkillOp);
    SetDropHighlighted(bCanAccept);
}

bool USkillSlotWidget::NativeOnDragOver(const FGeometry& Geo, const FDragDropEvent& E, UDragDropOperation* Op)
{
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp)
    {
        return false;
    }

    return CanAcceptSkillDrop(SkillOp);
}

bool USkillSlotWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp || !CanAcceptSkillDrop(SkillOp))
    {
        ResetDragVisualState();
        return false;
    }

    OnSkillDroppedOnSlot.Broadcast(SlotIndex, SkillOp->SkillData, GetSlotHotkey());
    
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Dropped skill '%s'"),
        SlotIndex, *SkillOp->SkillData.networkData.skillSlug);
    
    ResetDragVisualState();
    return true;
}

void USkillSlotWidget::NativeOnDragLeave(const FDragDropEvent& E, UDragDropOperation* Op)
{
    Super::NativeOnDragLeave(E, Op);
    
    // Only reset if this is the operation we're tracking
    if (ActiveDragOp.Get() == Op)
    {
        ResetDragVisualState();
    }
}

void USkillSlotWidget::NativeOnDragCancelled(const FDragDropEvent& E, UDragDropOperation* Op)
{
    ResetDragVisualState();
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
    if (!DragDropOp || !SkillManager)
    {
        return false;
    }
    
    // Use cached result for the same operation
    if (bCacheValid && CachedDragOp.Get() == DragDropOp)
    {
        return bCachedCanAccept;
    }

    const FString& SkillSlug = DragDropOp->SkillData.networkData.skillSlug;
    bool bHasSkill = SkillManager->HasSkill(SkillSlug);
    
    CachedDragOp = DragDropOp;
    bCachedCanAccept = bHasSkill;
    bCacheValid = true;
    
    return bHasSkill;
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
    
    if (SkillButton)
    {
        SkillButton->SetVisibility(ESlateVisibility::Visible);
    }
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
    ActiveDragOp.Reset();
    InvalidateDragCache();

    if (bIsDropHighlighted)
    {
        bIsDropHighlighted = false;
        UpdateVisualState();
    }

    if (SkillButton)
    {
        SkillButton->SetVisibility(ESlateVisibility::Visible);
    }
}

void USkillSlotWidget::BeginDestroy()
{
    // Unsubscribe from events before destruction
    if (SkillManager)
    {
        SkillManager->OnSkillCooldownStarted.RemoveDynamic(this, &USkillSlotWidget::OnSkillCooldownStarted);
        SkillManager->OnSkillReady.RemoveDynamic(this, &USkillSlotWidget::OnSkillReady);
    }
    
    Super::BeginDestroy();
}

void USkillSlotWidget::OnSkillCooldownStarted(const FString& SkillSlug)
{
    if (SkillSlug == AssignedSkillSlug && SkillManager)
    {
        CooldownTotalTime = CurrentSkillData.networkData.cooldownMs / 1000.0f;
        CooldownRemainingTime = SkillManager->GetSkillCooldownRemaining(SkillSlug);
        bIsOnCooldown = (CooldownRemainingTime > 0.0f);

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