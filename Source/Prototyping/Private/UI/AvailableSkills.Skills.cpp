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

void UAvailableSkillsWidget::SkillInitialize(UMyGameInstance* InGameInstance)
{
    if (!InGameInstance)
    {
        UE_LOG(LogGameplay, Error, TEXT("AvailableSkillsWidget: Cannot initialize with null GameInstance"));
        return;
    }

    GameInstance = InGameInstance;
    SkillManager = GameInstance->GetPlayerSkillManager();

    if (SkillManager)
    {
        SubscribeToSkillManagerEvents();
        RefreshSkillList();
        UE_LOG(LogGameplay, Log, TEXT("AvailableSkillsWidget: Initialized with PlayerSkillManager"));
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("AvailableSkillsWidget: PlayerSkillManager not available"));
    }
}

void UAvailableSkillsWidget::RefreshSkillList()
{
    if (!SkillManager)
    {
        UE_LOG(LogGameplay, Warning, TEXT("AvailableSkillsWidget: Cannot refresh - SkillManager not available"));
        return;
    }

    TArray<FPlayerSkillData> AllSkills = SkillManager->GetAllPlayerSkills();
    PopulateSkillList(AllSkills);
    
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Refreshed skill list with %d skills"), AllSkills.Num());
}

void UAvailableSkillsWidget::ShowWidget()
{
    if (!bIsVisible)
    {
        bIsVisible = true;
        // SelfHitTestInvisible: the panel and its children are visible and
        // interactive, but the panel's own background doesn't block drag events
        // that should reach lower-Z widgets (the skill bar).
        SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        
        // НЕ управляем курсором здесь - это делает UIManager
        // Уведомляем UIManager об изменении видимости
        if (OnWidgetVisibilityChanged.IsBound())
        {
            OnWidgetVisibilityChanged.Broadcast(true);
        }
        
        UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Widget shown - Skills: %d"), SkillItemWidgets.Num());
    }
}

void UAvailableSkillsWidget::HideWidget()
{
    bIsVisible = false;
    bDragging = false;
    bIsSkillDragInProgress = false;
    SetVisibility(ESlateVisibility::Collapsed);
    
    // Hide tooltip when widget is hidden
    HideSkillTooltip();
    
    // НЕ управляем курсором здесь - это делает UIManager
    // Уведомляем UIManager об изменении видимости
    if (OnWidgetVisibilityChanged.IsBound())
    {
        OnWidgetVisibilityChanged.Broadcast(false);
    }
    
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Widget hidden"));
}

void UAvailableSkillsWidget::FilterSkillsByType(ESkillEffectType EffectType)
{
    CurrentEffectTypeFilter = EffectType;
    bHasActiveFilters = (EffectType != ESkillEffectType::None) || (CurrentSchoolFilter != ESkillSchool::None);
    
    RefreshSkillList();
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Filtered by effect type: %d"), (int32)EffectType);
}

void UAvailableSkillsWidget::FilterSkillsBySchool(ESkillSchool School)
{
    CurrentSchoolFilter = School;
    bHasActiveFilters = (CurrentEffectTypeFilter != ESkillEffectType::None) || (School != ESkillSchool::None);
    
    RefreshSkillList();
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Filtered by school: %d"), (int32)School);
}

void UAvailableSkillsWidget::ClearFilters()
{
    CurrentEffectTypeFilter = ESkillEffectType::None;
    CurrentSchoolFilter = ESkillSchool::None;
    bHasActiveFilters = false;
    
    RefreshSkillList();
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Cleared all filters"));
}

void UAvailableSkillsWidget::OnClearFiltersClicked()
{
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Clear filters clicked"));
    ClearFilters();
}

void UAvailableSkillsWidget::OnCloseClicked()
{
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Close clicked"));
    HideWidget();
    
    // Уведомляем UIManager об изменении видимости
    if (OnWidgetVisibilityChanged.IsBound())
    {
        OnWidgetVisibilityChanged.Broadcast(false);
    }
}

void UAvailableSkillsWidget::OnPlayerSkillsInitialized(const TArray<FPlayerSkillData>& Skills)
{
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Player skills initialized, refreshing list"));
    PopulateSkillList(Skills);
}

void UAvailableSkillsWidget::OnSkillItemClicked(const FPlayerSkillData& SkillData)
{
    OnSkillSelected.Broadcast(SkillData);
    UE_LOG(LogGameplay, Log, TEXT("AvailableSkillsWidget: Skill selected: %s"), *SkillData.networkData.skillSlug);
}

void UAvailableSkillsWidget::OnSkillItemHovered(const FPlayerSkillData& SkillData, bool bIsHovered)
{
    if (bIsHovered)
    {
        HoveredSkillData = SkillData;
        
        // Get mouse position and show tooltip
        FVector2D MousePosition = FVector2D::ZeroVector;
        if (GetWorld() && GetWorld()->GetFirstPlayerController())
        {
            GetWorld()->GetFirstPlayerController()->GetMousePosition(MousePosition.X, MousePosition.Y);
        }
        ShowSkillTooltip(SkillData, MousePosition);
    }
    else
    {
        HideSkillTooltip();
    }
}

void UAvailableSkillsWidget::SubscribeToSkillManagerEvents()
{
    if (!SkillManager)
    {
        UE_LOG(LogGameplay, Warning, TEXT("AvailableSkillsWidget: Cannot subscribe - SkillManager is null"));
        return;
    }

    SkillManager->OnSkillsInitialized.AddDynamic(this, &UAvailableSkillsWidget::OnPlayerSkillsInitialized);

    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Subscribed to SkillManager events"));
}

void UAvailableSkillsWidget::UnsubscribeFromSkillManagerEvents()
{
    if (!SkillManager)
    {
        return;
    }

    // Unsubscribe from skill manager events
    SkillManager->OnSkillsInitialized.RemoveDynamic(this, &UAvailableSkillsWidget::OnPlayerSkillsInitialized);
    
    UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Unsubscribed from SkillManager events"));
}

UPanelWidget* UAvailableSkillsWidget::GetSkillContainer() const
{
    // Use WrapBox for grid layout if enabled and available, otherwise use ScrollBox
    if (bUseGridLayout && WrapBox_SkillItems)
    {
        return WrapBox_SkillItems;
    }
    else if (ScrollBox_SkillItems)
    {
        return ScrollBox_SkillItems;
    }
    
    return nullptr;
}

