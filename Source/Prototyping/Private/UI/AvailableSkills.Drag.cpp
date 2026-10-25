#include "UI/AvailableSkillsWidget.h"
#include "UI/AvailableSkillsWidget.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "UI/SkillDragDropOperation.h"
#include "UI/SkillDragVisualWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Input/Events.h"

void UAvailableSkillsWidget::OnChildSkillDragStarted_Handler(const FPlayerSkillData& SkillData)
{
    OnChildSkillDragStarted();
}

void UAvailableSkillsWidget::OnChildSkillDragEnded_Handler(const FPlayerSkillData& SkillData)
{
    OnChildSkillDragEnded();
}

void UAvailableSkillsWidget::OnChildSkillDragStarted()
{
    if (bIsSkillDragInProgress || !bIsVisible)
    {
        return;
    }
    bIsSkillDragInProgress = true;

    HideSkillTooltip();

    // Make the entire panel hit-test-invisible so Slate routes drag events
    // (DragOver / Drop) to the SkillBar widgets below us (Z = 10) instead
    // of stopping at our Z = 50 panel.
    SetVisibility(ESlateVisibility::HitTestInvisible);

    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Skill drag started — panel is now HitTestInvisible"));
}

void UAvailableSkillsWidget::OnChildSkillDragEnded()
{
    if (!bIsSkillDragInProgress)
    {
        return;
    }
    bIsSkillDragInProgress = false;

    if (bIsVisible)
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Skill drag ended — panel restored"));
}
