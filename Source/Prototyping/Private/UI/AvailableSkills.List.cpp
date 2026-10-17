#include "UI/AvailableSkillsWidget.h"
#include "UI/AvailableSkillsWidget.h"
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

void UAvailableSkillsWidget::PopulateSkillList(const TArray<FPlayerSkillData>& Skills)
{
    ClearSkillList();

    UPanelWidget* Container = GetSkillContainer();
    if (!Container)
    {
        UE_LOG(LogTemp, Error, TEXT("AvailableSkillsWidget: No skill container found"));
        return;
    }

    int32 DisplayedSkillCount = 0;

    // Configure WrapBox if using grid layout
    if (bUseGridLayout && WrapBox_SkillItems)
    {
        WrapBox_SkillItems->SetInnerSlotPadding(FVector2D(SlotGap, SlotGap));
    }

    for (const FPlayerSkillData& SkillData : Skills)
    {
        if (bHasActiveFilters && !PassesFilters(SkillData))
        {
            continue;
        }

        USkillItemWidget* SkillItemWidget = CreateSkillItemWidget(SkillData);
        if (SkillItemWidget)
        {
            SkillItemWidgets.Add(SkillItemWidget);
            
            if (bUseGridLayout && WrapBox_SkillItems)
            {
                // Add to WrapBox with slot configuration
                if (UWrapBoxSlot* WrapSlot = WrapBox_SkillItems->AddChildToWrapBox(SkillItemWidget))
                {
                    WrapSlot->SetPadding(FMargin(SlotGap));
                    WrapSlot->SetHorizontalAlignment(HAlign_Left);
                    WrapSlot->SetVerticalAlignment(VAlign_Top);
                    WrapSlot->SetFillEmptySpace(false);
                }
            }
            else if (ScrollBox_SkillItems)
            {
                // Add to ScrollBox
                ScrollBox_SkillItems->AddChild(SkillItemWidget);
            }
            
            DisplayedSkillCount++;
        }
    }

    UpdateSkillCountDisplay();
    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Populated %d skills using %s layout"), 
        DisplayedSkillCount, bUseGridLayout ? TEXT("Grid") : TEXT("List"));
}

void UAvailableSkillsWidget::ClearSkillList()
{
    UPanelWidget* Container = GetSkillContainer();
    if (Container)
    {
        Container->ClearChildren();
    }

    SkillItemWidgets.Empty();
}

USkillItemWidget* UAvailableSkillsWidget::CreateSkillItemWidget(const FPlayerSkillData& SkillData)
{
    if (!SkillItemWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("AvailableSkillsWidget: SkillItemWidgetClass not set"));
        return nullptr;
    }

    USkillItemWidget* ItemWidget = CreateWidget<USkillItemWidget>(this, SkillItemWidgetClass);
    if (ItemWidget)
    {
        ItemWidget->SetSkillData(SkillData);
        
        // Configure for grid layout if enabled
        if (bUseGridLayout)
        {
            ItemWidget->SetSlotSize(SlotSize.X); // Use X component for both width and height in grid
        }
        
        ItemWidget->OnSkillItemClicked.AddDynamic(this, &UAvailableSkillsWidget::OnSkillItemClicked);
        ItemWidget->OnSkillItemHovered.AddDynamic(this, &UAvailableSkillsWidget::OnSkillItemHovered);
        ItemWidget->OnSkillItemDragStarted.AddDynamic(this, &UAvailableSkillsWidget::OnChildSkillDragStarted_Handler);
        ItemWidget->OnSkillItemDragEnded.AddDynamic(this, &UAvailableSkillsWidget::OnChildSkillDragEnded_Handler);
        
        UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Created skill item widget for %s"), *SkillData.networkData.skillSlug);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AvailableSkillsWidget: Failed to create skill item widget for %s"), *SkillData.networkData.skillSlug);
    }

    return ItemWidget;
}

bool UAvailableSkillsWidget::PassesFilters(const FPlayerSkillData& SkillData) const
{
    if (CurrentEffectTypeFilter != ESkillEffectType::None && 
        SkillData.definitionData.effectType != CurrentEffectTypeFilter)
    {
        return false;
    }

    if (CurrentSchoolFilter != ESkillSchool::None && 
        SkillData.definitionData.school != CurrentSchoolFilter)
    {
        return false;
    }

    return true;
}

void UAvailableSkillsWidget::UpdateSkillCountDisplay()
{
    if (TextBlock_SkillCount)
    {
        FString CountText = FString::Printf(TEXT("Skills: %d"), SkillItemWidgets.Num());
        if (bHasActiveFilters)
        {
            CountText += TEXT(" (Filtered)");
        }
        TextBlock_SkillCount->SetText(FText::FromString(CountText));
    }

    // Enable/disable clear filters button based on whether filters are active
    if (Button_ClearFilters)
    {
        Button_ClearFilters->SetIsEnabled(bHasActiveFilters);
		Button_ClearFilters->SetVisibility(bHasActiveFilters ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UAvailableSkillsWidget::CreateSkillItemWidgets()
{
    // This method is kept for potential future use but not currently called
    // The actual widget creation happens in PopulateSkillList
}

// =========================
// TOOLTIP METHODS
// =========================

void UAvailableSkillsWidget::ShowSkillTooltip(const FPlayerSkillData& SkillData, FVector2D Position)
{
    if (SkillTooltipWidget)
    {
        SkillTooltipWidget->SetSkillData(SkillData);
        SkillTooltipWidget->UpdateTooltipPosition(Position);
        SkillTooltipWidget->ShowTooltip();
        bIsShowingTooltip = true;
        
        UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Showing tooltip for skill %s"), 
            *SkillData.networkData.skillSlug);
    }
}

void UAvailableSkillsWidget::HideSkillTooltip()
{
    if (SkillTooltipWidget && bIsShowingTooltip)
    {
        SkillTooltipWidget->HideTooltip();
        bIsShowingTooltip = false;
        
        UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Hiding skill tooltip"));
    }
}

void UAvailableSkillsWidget::UpdateTooltipPosition()
{
    if (SkillTooltipWidget && bIsShowingTooltip)
    {
        FVector2D MousePosition;
        if (GetWorld() && GetWorld()->GetFirstPlayerController())
        {
            GetWorld()->GetFirstPlayerController()->GetMousePosition(MousePosition.X, MousePosition.Y);
            SkillTooltipWidget->UpdateTooltipPosition(MousePosition);
        }
    }
}

