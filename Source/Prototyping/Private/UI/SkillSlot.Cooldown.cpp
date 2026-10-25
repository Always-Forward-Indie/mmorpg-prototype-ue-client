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
    UE_LOG(LogGameplay, Warning,
        TEXT("SkillSlotWidget[%d]: OnSkillCooldownStarted '%s' assigned='%s' match=%d"),
        SlotIndex, *SkillSlug, *AssignedSkillSlug,
        (SkillSlug == AssignedSkillSlug) ? 1 : 0);
    if (SkillSlug == AssignedSkillSlug && SkillManager)
    {
        CooldownTotalTime = CurrentSkillData.networkData.cooldownMs / 1000.0f;
        CooldownRemainingTime = SkillManager->GetSkillCooldownRemaining(SkillSlug);
        bIsOnCooldown = (CooldownRemainingTime > 0.0f);

        UE_LOG(LogGameplay, Warning,
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
        
        UE_LOG(LogGameplay, Log, TEXT("SkillSlotWidget[%d]: Skill %s is ready"), SlotIndex, *SkillSlug);
    }
}
