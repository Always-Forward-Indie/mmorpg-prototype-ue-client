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
    bMousePressed = false; // Initialize the mouse pressed state
    CooldownRemainingTime = 0.0f;
    CooldownTotalTime = 0.0f;
    
    // Initialize drag & drop debouncing
    LastDragEnterTime = 0.0f;
    LastDragLeaveTime = 0.0f;
}

void USkillSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Bind button events
    if (SkillButton)
    {
        SkillButton->OnClicked.AddDynamic(this, &USkillSlotWidget::OnSkillButtonClicked);
        SkillButton->OnPressed.AddDynamic(this, &USkillSlotWidget::OnSkillButtonPressed);
        SkillButton->OnReleased.AddDynamic(this, &USkillSlotWidget::OnSkillButtonReleased);
    }

    // Initialize visual state
    UpdateVisualState();
}

void USkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Update cooldown if skill manager is available
    if (SkillManager && !AssignedSkillSlug.IsEmpty())
    {
        bool bWasOnCooldown = bIsOnCooldown;
        bIsOnCooldown = SkillManager->IsSkillOnCooldown(AssignedSkillSlug);
        
        if (bIsOnCooldown)
        {
            CooldownRemainingTime = SkillManager->GetSkillCooldownRemaining(AssignedSkillSlug);
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
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: DragEnter - not a skill operation"), SlotIndex);
        return;
    }
    
    // Debouncing: игнорируем слишком частые события DragEnter
    float CurrentTime = GetCurrentTime();
    if (CurrentTime - LastDragEnterTime < DragEventDebounceTime)
    {
        UE_LOG(LogTemp, Verbose, TEXT("SkillSlotWidget[%d]: DragEnter - Debounced (too soon)"), SlotIndex);
        return;
    }
    LastDragEnterTime = CurrentTime;
    
    // Защита от повторных вызовов DragEnter для той же операции
    if (ActiveDragOp.Get() == Op)
    {
        UE_LOG(LogTemp, Verbose, TEXT("SkillSlotWidget[%d]: DragEnter - Already handling this operation, ignoring"), SlotIndex);
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: DragEnter - Skill: %s, Op: %p"), 
        SlotIndex, *SkillOp->SkillData.networkData.skillSlug, Op);
    
    // Сбрасываем кэш при новой операции
    InvalidateDragCache();
    
    // Устанавливаем активную операцию
    ActiveDragOp = Op;
    
    // Скрываем кнопку для визуального эффекта
    if (SkillButton) 
    {
        SkillButton->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    
    // Проверяем и устанавливаем highlight один раз при входе
    bool bCanAccept = CanAcceptSkillDrop(SkillOp);
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: DragEnter - CanAccept: %s"), 
        SlotIndex, bCanAccept ? TEXT("TRUE") : TEXT("FALSE"));
    SetDropHighlighted(bCanAccept);
}

bool USkillSlotWidget::NativeOnDragOver(const FGeometry& Geo, const FDragDropEvent& E, UDragDropOperation* Op)
{
    // Не вызываем SetDropHighlighted здесь - состояние уже установлено в DragEnter
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp)
    {
        return false;
    }

    // Просто возвращаем кэшированный результат
    return CanAcceptSkillDrop(SkillOp);
}

bool USkillSlotWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Drop started"), SlotIndex);
    
    auto* SkillOp = Cast<USkillDragDropOperation>(Op);
    if (!SkillOp) 
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: Drop failed - not a skill operation"), SlotIndex);
        // Сбрасываем состояние при ошибке
        SetDropHighlighted(false);
        InvalidateDragCache();
        if (SkillButton) SkillButton->SetVisibility(ESlateVisibility::Visible);
        return false;
    }

    if (!CanAcceptSkillDrop(SkillOp)) 
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: Drop failed - cannot accept skill"), SlotIndex);
        // Сбрасываем состояние при ошибке
        SetDropHighlighted(false);
        InvalidateDragCache();
        if (SkillButton) SkillButton->SetVisibility(ESlateVisibility::Visible);
        return false;
    }

    // Выполняем дроп
    OnSkillDroppedOnSlot.Broadcast(SlotIndex, SkillOp->SkillData, GetSlotHotkey());
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Successfully dropped skill %s"), 
        SlotIndex, *SkillOp->SkillData.networkData.skillSlug);
    
    // Сбрасываем состояние после успешного дропа
    SetDropHighlighted(false);
    InvalidateDragCache();
    if (SkillButton) 
    {
        SkillButton->SetVisibility(ESlateVisibility::Visible);
    }
    
    return true;
}

void USkillSlotWidget::NativeOnDragLeave(const FDragDropEvent& E, UDragDropOperation* Op)
{
    Super::NativeOnDragLeave(E, Op);
    
    // Debouncing: игнорируем слишком частые события DragLeave
    float CurrentTime = GetCurrentTime();
    if (CurrentTime - LastDragLeaveTime < DragEventDebounceTime)
    {
        UE_LOG(LogTemp, Verbose, TEXT("SkillSlotWidget[%d]: DragLeave - Debounced (too soon)"), SlotIndex);
        return;
    }
    LastDragLeaveTime = CurrentTime;
    
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: DragLeave - Op: %p, ActiveOp: %p"), 
        SlotIndex, Op, ActiveDragOp.Get());
    
    // Проверяем, что это наша операция
    if (ActiveDragOp.Get() == Op)
    {
        // Сбрасываем состояние когда курсор покидает слот
        ActiveDragOp.Reset();
        InvalidateDragCache();
        SetDropHighlighted(false);
        
        // Восстанавливаем видимость кнопки
        if (SkillButton) 
        {
            SkillButton->SetVisibility(ESlateVisibility::Visible);
        }
        
        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: DragLeave - State reset, highlight removed"), SlotIndex);
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("SkillSlotWidget[%d]: DragLeave - Operation mismatch, ignoring"), SlotIndex);
    }
}

void USkillSlotWidget::NativeOnDragCancelled(const FDragDropEvent& E, UDragDropOperation* Op)
{
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Drag operation cancelled"), SlotIndex);
    
    // При отмене операции принудительно сбрасываем все состояния
    ActiveDragOp.Reset();
    InvalidateDragCache();
    SetDropHighlighted(false);
    
    if (SkillButton) 
    {
        SkillButton->SetVisibility(ESlateVisibility::Visible);
    }
}


void USkillSlotWidget::Initialize(int32 InSlotIndex, UPlayerSkillManager* InSkillManager)
{
    SlotIndex = InSlotIndex;
    SkillManager = InSkillManager;

    if (SkillManager)
    {
        // Get current slot data
        FSkillSlotData SlotData = SkillManager->GetSkillSlot(SlotIndex);
        SetSlotData(SlotData);

        UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget: Initialized slot %d"), SlotIndex);
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
    // Добавляем защиту от лишних обновлений
    if (bIsDropHighlighted == bDropHighlighted)
    {
        return; // Состояние уже установлено, не обновляем
    }
    
    // Дополнительная защита от слишком частых изменений highlight
    static float LastHighlightChangeTime = 0.0f;
    float CurrentTime = GetCurrentTime();
    const float MinHighlightChangeInterval = 0.02f; // 20ms минимум между изменениями
    
    if (CurrentTime - LastHighlightChangeTime < MinHighlightChangeInterval)
    {
        UE_LOG(LogTemp, Verbose, TEXT("SkillSlotWidget[%d]: SetDropHighlighted blocked - too frequent"), SlotIndex);
        return;
    }
    
    LastHighlightChangeTime = CurrentTime;
    
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: SetDropHighlighted %s -> %s"),
        SlotIndex, 
        bIsDropHighlighted ? TEXT("TRUE") : TEXT("FALSE"),
        bDropHighlighted ? TEXT("TRUE") : TEXT("FALSE"));
    
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

    // Update drop highlight border with improved logging
    if (DropHighlightBorder)
    {
        ESlateVisibility NewVisibility = bIsDropHighlighted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden;
        DropHighlightBorder->SetVisibility(NewVisibility);
        
        if (bIsDropHighlighted)
        {
            DropHighlightBorder->SetColorAndOpacity(DropHighlightColor);
            UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: DROP HIGHLIGHT SHOWN"), SlotIndex);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: DROP HIGHLIGHT HIDDEN"), SlotIndex);
        }
    }
    else if (bIsDropHighlighted)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillSlotWidget[%d]: DropHighlightBorder is NULL but trying to show highlight!"), SlotIndex);
    }

    // Update cooldown overlay
    if (CooldownOverlay)
    {
        CooldownOverlay->SetVisibility(bIsOnCooldown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
    }
}

void USkillSlotWidget::UpdateCooldownDisplay()
{
    // Update cooldown progress bar
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

    // Update cooldown text
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
    if (HotkeyText)
    {
        if (CurrentSlotData.boundKey.IsValid())
        {
            FString HotkeyString = CurrentSlotData.boundKey.ToString();
            HotkeyText->SetText(FText::FromString(HotkeyString));
            HotkeyText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            HotkeyText->SetVisibility(ESlateVisibility::Hidden);
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
        UE_LOG(LogTemp, Error, TEXT("SkillSlotWidget[%d]: CanAccept - NULL operation"), SlotIndex);
        return false;
    }
    
    // Проверяем кэш
    if (bCacheValid && CachedDragOp.Get() == DragDropOp)
    {
        UE_LOG(LogTemp, Verbose, TEXT("SkillSlotWidget[%d]: CanAccept - Using cached result: %s"), 
            SlotIndex, bCachedCanAccept ? TEXT("TRUE") : TEXT("FALSE"));
        return bCachedCanAccept;
    }
    
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillSlotWidget[%d]: CanAccept - NULL SkillManager"), SlotIndex);
        return false;
    }

    // Вычисляем и кэшируем результат
    const FString& SkillSlug = DragDropOp->SkillData.networkData.skillSlug;
    bool bHasSkill = SkillManager->HasSkill(SkillSlug);
    
    // Обновляем кэш
    CachedDragOp = DragDropOp;
    bCachedCanAccept = bHasSkill;
    bCacheValid = true;
    
    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: CanAccept - Skill: %s, HasSkill: %s (Cached)"), 
        SlotIndex, *SkillSlug, bHasSkill ? TEXT("TRUE") : TEXT("FALSE"));
    
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
    UE_LOG(LogTemp, Warning, TEXT("SkillSlotWidget[%d]: Force reset drag state called"), SlotIndex);

    // Принудительный сброс всех состояний drag & drop
    ActiveDragOp.Reset();
    InvalidateDragCache();

    // Принудительно убираем highlight
    if (bIsDropHighlighted)
    {
        bIsDropHighlighted = false;
        UpdateVisualState();
    }

    // Восстанавливаем видимость кнопки
    if (SkillButton)
    {
        SkillButton->SetVisibility(ESlateVisibility::Visible);
    }

    UE_LOG(LogTemp, Log, TEXT("SkillSlotWidget[%d]: Force reset completed"), SlotIndex);
}