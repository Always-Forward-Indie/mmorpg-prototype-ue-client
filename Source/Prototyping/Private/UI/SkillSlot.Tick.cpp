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
            UE_LOG(LogGameplay, Error, TEXT("SkillSlotWidget[%d]: STUCK DRAG DETECTED — bIsDragging=true for %.1fs, force-resetting"),
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

