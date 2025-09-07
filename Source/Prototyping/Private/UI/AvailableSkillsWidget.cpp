#include "UI/AvailableSkillsWidget.h"
#include "MyGameInstance.h"
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
#include "Input/Events.h"

// ========== UAvailableSkillsWidget ==========

void UAvailableSkillsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    bIsVisible = false;
    SkillItemWidgets.Empty();

    // Set up for dragging
    SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
    SetAlignmentInViewport(FVector2D(0.f, 0.f));

    // Set initial position (center of screen)
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        
        // Center the widget
        CurrentViewportPosition = FVector2D((W - Size.X) * 0.5f, (H - Size.Y) * 0.5f);
        SetPositionInViewport(CurrentViewportPosition, false);
    }

    if (UWidget* Root = GetRootWidget())
    {
        // Корень больше не блокирует пустые зоны
        Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    // Create tooltip widget if class is set
    if (SkillTooltipWidgetClass)
    {
        SkillTooltipWidget = CreateWidget<USkillTooltipWidget>(this, SkillTooltipWidgetClass);
        if (SkillTooltipWidget)
        {
            SkillTooltipWidget->AddToViewport(1000); // High Z-order for tooltip
            SkillTooltipWidget->SetVisibility(ESlateVisibility::Hidden);
            UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Created skill tooltip widget"));
        }
    }

    // Bind button events
    if (Button_ClearFilters)
    {
        Button_ClearFilters->OnClicked.AddDynamic(this, &UAvailableSkillsWidget::OnClearFiltersClicked);
    }

    if (Button_Close)
    {
        Button_Close->OnClicked.AddDynamic(this, &UAvailableSkillsWidget::OnCloseClicked);
    }

    // Initialize widget but hide it
    if (TextBlock_Title)
    {
        TextBlock_Title->SetText(FText::FromString(TEXT("Available Skills")));
    }

    UpdateSkillCountDisplay();
    HideWidget();

    if (SkillManager)
    {
        SubscribeToSkillManagerEvents();
    }
}

void UAvailableSkillsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Update tooltip position if visible
    if (bIsShowingTooltip)
    {
        UpdateTooltipPosition();
    }
}

void UAvailableSkillsWidget::NativeDestruct()
{
    // Clean up tooltip
    if (SkillTooltipWidget)
    {
        SkillTooltipWidget->RemoveFromParent();
        SkillTooltipWidget = nullptr;
    }

    UnsubscribeFromSkillManagerEvents();
    Super::NativeDestruct();
}

FReply UAvailableSkillsWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsVisible && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Hide tooltip when clicking anywhere in the available skills window
        HideSkillTooltip();
        
        bool bShouldStartDrag = false;

        if (DragHandle)
        {
            // Check if clicking on drag handle
            const FGeometry DragHandleGeometry = DragHandle->GetCachedGeometry();
            const FVector2D LocalMousePos = DragHandleGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D DragHandleSize = DragHandleGeometry.GetLocalSize();
            
            bShouldStartDrag = (LocalMousePos.X >= 0 && LocalMousePos.X <= DragHandleSize.X &&
                LocalMousePos.Y >= 0 && LocalMousePos.Y <= DragHandleSize.Y);
        }
        else
        {
            // No specific drag handle, allow dragging from title area
            bShouldStartDrag = true;
        }

        if (bShouldStartDrag)
        {
            const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
            const FVector2D Screen = InMouseEvent.GetScreenSpacePosition();
            const FVector2D MouseVP = Screen / Scale;
            
            DragOffset = MouseVP - CurrentViewportPosition;
            bDragging = true;

            if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            {
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            }
            return FReply::Handled();
        }
    }
    
    // Hide tooltip on right mouse button click as well
    if (bIsVisible && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        HideSkillTooltip();
    }
    
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UAvailableSkillsWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;

        if (TSharedPtr<SWidget> Slate = GetCachedWidget())
        {
            return FReply::Handled().ReleaseMouseCapture();
        }
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UAvailableSkillsWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UAvailableSkillsWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPosAbs)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);

    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D ViewportSize = FVector2D(W, H) / Scale;

    ForceLayoutPrepass();
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(400, 300);

    // Convert cursor position to viewport space
    const FVector2D MouseVP = ScreenCursorPosAbs / Scale;

    // Calculate desired position
    FVector2D Pos = MouseVP - DragOffset;

    // Clamp to viewport bounds
    Pos.X = FMath::Clamp(Pos.X, DragPadding.Left, ViewportSize.X - Size.X - DragPadding.Right);
    Pos.Y = FMath::Clamp(Pos.Y, DragPadding.Top, ViewportSize.Y - Size.Y - DragPadding.Bottom);

    // Update position
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}

void UAvailableSkillsWidget::Initialize(UMyGameInstance* InGameInstance)
{
    if (!InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("AvailableSkillsWidget: Cannot initialize with null GameInstance"));
        return;
    }

    GameInstance = InGameInstance;
    SkillManager = GameInstance->GetPlayerSkillManager();

    if (SkillManager)
    {
        SubscribeToSkillManagerEvents();
        RefreshSkillList();
        UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Initialized with PlayerSkillManager"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: PlayerSkillManager not available"));
    }
}

void UAvailableSkillsWidget::RefreshSkillList()
{
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Cannot refresh - SkillManager not available"));
        return;
    }

    TArray<FPlayerSkillData> AllSkills = SkillManager->GetAllPlayerSkills();
    PopulateSkillList(AllSkills);
    
    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Refreshed skill list with %d skills"), AllSkills.Num());
}

void UAvailableSkillsWidget::ShowWidget()
{
    if (!bIsVisible)
    {
        bIsVisible = true;
        SetVisibility(ESlateVisibility::Visible);
        
        // Show mouse cursor when skills window is open
        if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        {
            PC->bShowMouseCursor = true;
            
            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
            PC->SetInputMode(InputMode);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Widget shown - Skills: %d"), SkillItemWidgets.Num());
    }
}

void UAvailableSkillsWidget::HideWidget()
{
    bIsVisible = false;
    bDragging = false;
    SetVisibility(ESlateVisibility::Hidden);
    
    // Hide tooltip when widget is hidden
    HideSkillTooltip();
    
    // Restore game-only input mode
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        PC->bShowMouseCursor = false;
        
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Widget hidden"));
}

void UAvailableSkillsWidget::FilterSkillsByType(ESkillEffectType EffectType)
{
    CurrentEffectTypeFilter = EffectType;
    bHasActiveFilters = (EffectType != ESkillEffectType::None) || (CurrentSchoolFilter != ESkillSchool::None);
    
    RefreshSkillList();
    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Filtered by effect type: %d"), (int32)EffectType);
}

void UAvailableSkillsWidget::FilterSkillsBySchool(ESkillSchool School)
{
    CurrentSchoolFilter = School;
    bHasActiveFilters = (CurrentEffectTypeFilter != ESkillEffectType::None) || (School != ESkillSchool::None);
    
    RefreshSkillList();
    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Filtered by school: %d"), (int32)School);
}

void UAvailableSkillsWidget::ClearFilters()
{
    CurrentEffectTypeFilter = ESkillEffectType::None;
    CurrentSchoolFilter = ESkillSchool::None;
    bHasActiveFilters = false;
    
    RefreshSkillList();
    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Cleared all filters"));
}

void UAvailableSkillsWidget::OnClearFiltersClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Clear filters clicked"));
    ClearFilters();
}

void UAvailableSkillsWidget::OnCloseClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Close clicked"));
    HideWidget();
}

void UAvailableSkillsWidget::OnPlayerSkillsInitialized(const TArray<FPlayerSkillData>& Skills)
{
    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Player skills initialized, refreshing list"));
    PopulateSkillList(Skills);
}

void UAvailableSkillsWidget::OnSkillItemClicked(const FPlayerSkillData& SkillData)
{
    OnSkillSelected.Broadcast(SkillData);
    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Skill selected: %s"), *SkillData.networkData.skillSlug);
}

void UAvailableSkillsWidget::OnSkillItemHovered(const FPlayerSkillData& SkillData, bool bIsHovered)
{
    if (bIsHovered)
    {
        HoveredSkillData = SkillData;
        
        // Get mouse position and show tooltip
        FVector2D MousePosition;
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
        UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Cannot subscribe - SkillManager is null"));
        return;
    }

    SkillManager->OnSkillsInitialized.AddDynamic(this, &UAvailableSkillsWidget::OnPlayerSkillsInitialized);

    UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Subscribed to SkillManager events"));
}

void UAvailableSkillsWidget::UnsubscribeFromSkillManagerEvents()
{
    if (!SkillManager)
    {
        return;
    }

    // Unsubscribe from skill manager events
    SkillManager->OnSkillsInitialized.RemoveDynamic(this, &UAvailableSkillsWidget::OnPlayerSkillsInitialized);
    
    UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Unsubscribed from SkillManager events"));
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

// ========== USkillItemWidget ==========

void USkillItemWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Enable mouse events for this widget
    SetVisibility(ESlateVisibility::Visible);
    
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: NativeConstruct called for skill %s"), *CurrentSkillData.networkData.skillSlug);

    // Configure main border for visual feedback
    if (SkillBorder)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: SkillBorder found and configured"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: SkillBorder is NULL - make sure to bind it in UMG"));
    }
}

void USkillItemWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    SetSlotSize(SlotSize);
}

void USkillItemWidget::SetSkillData(const FPlayerSkillData& SkillData)
{
    CurrentSkillData = SkillData;
    UpdateVisualDisplay();
    
    UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: SetSkillData called for skill %s"), *SkillData.networkData.skillSlug);
}

void USkillItemWidget::SetSlotSize(float InSize)
{
    SlotSize = InSize;
    if (SlotSizeBox)
    {
        SlotSizeBox->SetWidthOverride(SlotSize);
        SlotSizeBox->SetHeightOverride(SlotSize);
    }
}

void USkillItemWidget::OnSkillClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Skill clicked for %s"), *CurrentSkillData.networkData.skillSlug);
    OnSkillItemClicked.Broadcast(CurrentSkillData);
}

void USkillItemWidget::SimulateClick()
{
    OnSkillClicked();
}

FReply USkillItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Mouse button down - Button: %s, Skill: %s"), 
        *InMouseEvent.GetEffectingButton().ToString(), *CurrentSkillData.networkData.skillSlug);

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Left mouse button detected, preparing for drag"));
        
        // Update visual state to show pressed
        UpdateClickState(true);
        
        // DetectDrag автоматически вызовет NativeOnDragDetected при достижении порога
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }
    
    return FReply::Unhandled();
}

FReply USkillItemWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Просто делегируем родительскому классу
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USkillItemWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Mouse button up - Button: %s, Skill: %s"), 
        *InMouseEvent.GetEffectingButton().ToString(), *CurrentSkillData.networkData.skillSlug);
    
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Reset mouse tracking
        bMousePressed = false;
        
        // Reset visual state
        UpdateClickState(false);
        
        // Handle click if we didn't start dragging
        if (!bIsDragging)
        {
            // Check if mouse is still over the widget
            FVector2D LocalMousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            if (InGeometry.GetLocalSize().X > 0 && InGeometry.GetLocalSize().Y > 0)
            {
                if (LocalMousePosition.X >= 0 && LocalMousePosition.X <= InGeometry.GetLocalSize().X &&
                    LocalMousePosition.Y >= 0 && LocalMousePosition.Y <= InGeometry.GetLocalSize().Y)
                {
                    SimulateClick();
                }
            }
        }
        
        bIsDragging = false;
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

// Правильная реализация NativeOnDragDetected
void USkillItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL DRAG DETECTED DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: DRAG DETECTED for skill %s"), *CurrentSkillData.networkData.skillSlug);

    // Hide tooltip when starting drag operation
    OnSkillItemHovered.Broadcast(CurrentSkillData, false);

    // Создаем DragDropOperation
    USkillDragDropOperation* DragDropOp = NewObject<USkillDragDropOperation>(this, USkillDragDropOperation::StaticClass());
    if (!DragDropOp)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillItemWidget: ❌ Failed to create DragDropOperation"));
        OutOperation = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("=== SKILL DRAG DETECTED DEBUG END (FAILED) ==="));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ✅ Created DragDropOperation successfully"));

    // Настраиваем операцию
    DragDropOp->SetSkillData(CurrentSkillData, this);
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ✅ Set skill data in DragDropOperation"));
    
    // НОВОЕ: Передаем класс drag visual из USkillItemWidget в DragDropOperation
    if (DragVisualWidgetClass)
    {
        DragDropOp->DragVisualWidgetClass = DragVisualWidgetClass;
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Set custom DragVisualWidgetClass: %s"), *DragVisualWidgetClass->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: No custom DragVisualWidgetClass set"));
    }
    
    // Создаем drag visual
    UUserWidget* DragVisual = DragDropOp->CreateDragVisualWidget();
    if (DragVisual)
    {
        DragDropOp->DefaultDragVisual = DragVisual;
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ✅ Created custom drag visual"));
    }
    else
    {
        // Fallback - используем clone текущего виджета
        DragDropOp->DefaultDragVisual = this;
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Using self as drag visual (fallback)"));
    }
    
    // Настройки визуала
    DragDropOp->Pivot = EDragPivot::MouseDown;
    DragDropOp->Offset = FVector2D::ZeroVector;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ✅ Configured drag operation settings"));
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ✅ Setting OutOperation and marking as dragging"));
    
    OutOperation = DragDropOp;
    bIsDragging = true;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ✅ Started dragging skill %s"), *CurrentSkillData.networkData.skillSlug);
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL DRAG DETECTED DEBUG END (SUCCESS) ==="));
}

void USkillItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    
    bIsHovered = true;
    UpdateHoverState();
    
    // Broadcast hover event
    OnSkillItemHovered.Broadcast(CurrentSkillData, true);
    
    UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: Mouse entered skill %s"), *CurrentSkillData.networkData.skillSlug);
}

void USkillItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    
    bIsHovered = false;
    bIsDragging = false;
    UpdateHoverState();
    
    // Broadcast hover event
    OnSkillItemHovered.Broadcast(CurrentSkillData, false);
    
    UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: Mouse left skill %s"), *CurrentSkillData.networkData.skillSlug);
}

void USkillItemWidget::UpdateVisualDisplay()
{
    UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: UpdateVisualDisplay for skill %s"), 
        *CurrentSkillData.networkData.skillSlug);
    
    // Update skill icon
    if (SkillIcon)
    {
        bool bIconSet = false;
        
        if (CurrentSkillData.definitionData.skillIcon.IsValid())
        {
            UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: Attempting to load skill icon from TSoftObjectPtr"));
            
            if (UTexture2D* IconTexture = CurrentSkillData.definitionData.skillIcon.LoadSynchronous())
            {
                SkillIcon->SetBrushFromTexture(IconTexture);
                bIconSet = true;
                UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: Successfully set skill icon from texture"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Failed to load texture from TSoftObjectPtr"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Skill icon TSoftObjectPtr is not valid"));
        }
        
        if (!bIconSet && DefaultSkillIcon)
        {
            SkillIcon->SetBrushFromTexture(DefaultSkillIcon);
            bIconSet = true;
            UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: Set default skill icon"));
        }
        
        if (!bIconSet)
        {
            UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: No icon available, no texture set"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SkillItemWidget: SkillIcon widget is NULL"));
    }

    // Update skill level (keep this for quick reference)
    if (SkillLevelText)
    {
        FString LevelText = FString::Printf(TEXT("Lv.%d"), CurrentSkillData.networkData.skillLevel);
        SkillLevelText->SetText(FText::FromString(LevelText));
        
        // In compact layout, show level; in full layout, you might want to hide it
        if (bUseCompactLayout)
        {
            SkillLevelText->SetVisibility(ESlateVisibility::Visible);
        }
    }

    // Hide or show detailed elements based on layout mode
    if (SkillNameText)
    {
        if (bUseCompactLayout)
        {
            // Hide the name text in compact mode
            SkillNameText->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            // Show name in full layout mode
            FText DisplayName = CurrentSkillData.definitionData.displayName;
            if (DisplayName.IsEmpty())
            {
                DisplayName = FText::FromString(CurrentSkillData.networkData.skillSlug);
            }
            SkillNameText->SetText(DisplayName);
            SkillNameText->SetVisibility(ESlateVisibility::Visible);
        }
    }

    if (SkillDescriptionText)
    {
        if (bUseCompactLayout)
        {
            // Hide description in compact mode - it will be in tooltip
            SkillDescriptionText->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            // Show description in full layout mode
            SkillDescriptionText->SetText(CurrentSkillData.definitionData.description);
            SkillDescriptionText->SetVisibility(ESlateVisibility::Visible);
        }
    }

    if (CooldownText)
    {
        if (bUseCompactLayout)
        {
            // Hide cooldown in compact mode - it will be in tooltip
            CooldownText->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            // Show cooldown in full layout mode - use networkData.cooldownMs
            FString CooldownStr = FormatCooldownTime(CurrentSkillData.networkData.cooldownMs);
            CooldownText->SetText(FText::FromString(CooldownStr));
            CooldownText->SetVisibility(ESlateVisibility::Visible);
        }
    }

    if (ManaCostText)
    {
        if (bUseCompactLayout)
        {
            // Hide mana cost in compact mode - it will be in tooltip
            ManaCostText->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            // Show mana cost in full layout mode - use networkData.costMp
            FString ManaCostStr = FString::Printf(TEXT("%d MP"), CurrentSkillData.networkData.costMp);
            ManaCostText->SetText(FText::FromString(ManaCostStr));
            ManaCostText->SetVisibility(ESlateVisibility::Visible);
        }
    }

    // Update skill type indicator (keep for visual distinction)
    if (SkillTypeIndicator)
    {
        FLinearColor SchoolColor = GetSchoolColor(CurrentSkillData.definitionData.school);
        SkillTypeIndicator->SetColorAndOpacity(SchoolColor);
    }

    UpdateHoverState();
}

void USkillItemWidget::UpdateHoverState()
{
    if (SkillBorder)
    {
        FLinearColor BorderColor = bIsHovered ? HoverColor : NormalColor;
        SkillBorder->SetBrushColor(BorderColor);
    }
    
    if (SkillIcon)
    {
        FLinearColor IconColor = bIsHovered ? HoverColor : NormalColor;
        SkillIcon->SetColorAndOpacity(IconColor);
    }
}

void USkillItemWidget::UpdateClickState(bool bPressed)
{
    if (SkillBorder)
    {
        FLinearColor BorderColor = bPressed ? ClickedColor : (bIsHovered ? HoverColor : NormalColor);
        SkillBorder->SetBrushColor(BorderColor);
    }
    
    if (SkillIcon)
    {
        FLinearColor IconColor = bPressed ? ClickedColor : (bIsHovered ? HoverColor : NormalColor);
        SkillIcon->SetColorAndOpacity(IconColor);
    }
}

FLinearColor USkillItemWidget::GetSchoolColor(ESkillSchool School) const
{
    if (const FLinearColor* Color = SchoolColors.Find(School))
    {
        return *Color;
    }
    return FLinearColor::Gray; // Default color for unknown schools
}

FString USkillItemWidget::FormatCooldownTime(float TimeMs) const
{
    float TimeSeconds = TimeMs / 1000.0f;
    
    if (TimeSeconds < 1.0f)
    {
        return TEXT("Instant");
    }
    else if (TimeSeconds < 60.0f)
    {
        return FString::Printf(TEXT("%.1fs"), TimeSeconds);
    }
    else
    {
        int32 Minutes = static_cast<int32>(TimeSeconds / 60.0f);
        int32 Seconds = static_cast<int32>(TimeSeconds) % 60;
        return FString::Printf(TEXT("%dm %ds"), Minutes, Seconds);
    }
}
