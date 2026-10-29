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
        // Re-bind safe: SlotInitialize runs again when the bar is rebuilt, so
        // drop previous bindings first — otherwise every rebind double-fires
        // the handlers (double sounds, double refresh work per event).
        SkillManager->OnSkillCooldownStarted.RemoveDynamic(this, &USkillSlotWidget::OnSkillCooldownStarted);
        SkillManager->OnSkillReady.RemoveDynamic(this, &USkillSlotWidget::OnSkillReady);
        SkillManager->OnSkillCooldownStarted.AddDynamic(this, &USkillSlotWidget::OnSkillCooldownStarted);
        SkillManager->OnSkillReady.AddDynamic(this, &USkillSlotWidget::OnSkillReady);

        // Get current slot data
        FSkillSlotData SlotData = SkillManager->GetSkillSlot(SlotIndex);
        SetSlotData(SlotData);

        UE_LOG(LogGameplay, Log, TEXT("SkillSlotWidget: Initialized slot %d with event subscriptions"), SlotIndex);
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("SkillSlotWidget: Initialized slot %d without SkillManager"), SlotIndex);
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
            UE_LOG(LogGameplay, Verbose,
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

