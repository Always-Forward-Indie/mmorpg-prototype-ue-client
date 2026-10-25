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

void USkillSlotWidget::OnSkillButtonClicked()
{
    if (bIsEnabled && !AssignedSkillSlug.IsEmpty())
    {
        OnSkillSlotClicked.Broadcast(SlotIndex, AssignedSkillSlug);
        UE_LOG(LogGameplay, Verbose, TEXT("SkillSlotWidget: Clicked slot %d with skill %s"), SlotIndex, *AssignedSkillSlug);
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

