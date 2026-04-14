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
#include "Input/Events.h"

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
        const float InitScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
        const FVector2D VPSizeInit = FVector2D(W, H) / InitScale;
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        
        // Center the widget, clamp to viewport so it never opens off-screen
        CurrentViewportPosition = FVector2D(
            FMath::Max(0.f, (VPSizeInit.X - Size.X) * 0.5f),
            FMath::Max(0.f, (VPSizeInit.Y - Size.Y) * 0.5f));
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
        TextBlock_Title->SetText(FText::FromString(TEXT("Skills")));
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

    // Safety net: if Slate has no active drag-drop but we still think a skill
    // drag is in progress (source widget's MouseUp / DragCancelled was missed),
    // restore normal visibility so the panel doesn't stay stuck invisible.
    if (bIsSkillDragInProgress && !FSlateApplication::Get().IsDragDropping())
    {
        OnChildSkillDragEnded();
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

        // Debug: Log widget geometry information
        UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Mouse click - Widget Geometry: Size(%f, %f), Pos(%f, %f)"), 
            InGeometry.GetLocalSize().X, InGeometry.GetLocalSize().Y,
            InGeometry.GetAbsolutePosition().X, InGeometry.GetAbsolutePosition().Y);

        if (DragHandle)
        {
            // Check if clicking on drag handle
            const FGeometry DragHandleGeometry = DragHandle->GetCachedGeometry();
            
            // Debug: Log drag handle information
            UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: DragHandle found - Size(%f, %f), Pos(%f, %f), Visibility: %s"), 
                DragHandleGeometry.GetLocalSize().X, DragHandleGeometry.GetLocalSize().Y,
                DragHandleGeometry.GetAbsolutePosition().X, DragHandleGeometry.GetAbsolutePosition().Y,
                *UEnum::GetValueAsString(DragHandle->GetVisibility()));
            
            // Convert mouse position to local coordinates of the drag handle
            const FVector2D ScreenSpacePosition = InMouseEvent.GetScreenSpacePosition();
            const FVector2D LocalMousePos = DragHandleGeometry.AbsoluteToLocal(ScreenSpacePosition);
            const FVector2D DragHandleSize = DragHandleGeometry.GetLocalSize();
            
            // Check if mouse click is within drag handle bounds
            bShouldStartDrag = (LocalMousePos.X >= 0 && LocalMousePos.X <= DragHandleSize.X &&
                LocalMousePos.Y >= 0 && LocalMousePos.Y <= DragHandleSize.Y);
            
            UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: DragHandle click check - ScreenPos: (%f, %f), LocalPos: (%f, %f), Size: (%f, %f), ShouldDrag: %s"), 
                ScreenSpacePosition.X, ScreenSpacePosition.Y,
                LocalMousePos.X, LocalMousePos.Y, 
                DragHandleSize.X, DragHandleSize.Y, 
                bShouldStartDrag ? TEXT("true") : TEXT("false"));

            // Additional check: Is the DragHandle actually receiving input?
            if (DragHandleSize.X <= 0 || DragHandleSize.Y <= 0)
            {
                UE_LOG(LogTemp, Error, TEXT("AvailableSkillsWidget: DragHandle has zero or negative size! This will prevent interaction."));
                
                // Fallback: Use title area for dragging (top 40 pixels of widget)
                const FVector2D LocalClickPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
                bShouldStartDrag = (LocalClickPos.Y >= 0 && LocalClickPos.Y <= 40.0f);
                
                UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Using fallback title area drag - LocalClick: (%f, %f), ShouldDrag: %s"), 
                    LocalClickPos.X, LocalClickPos.Y, bShouldStartDrag ? TEXT("true") : TEXT("false"));
            }
        }
        else
        {
            // No specific drag handle, use title area for dragging (top 40 pixels of widget)
            const FVector2D LocalClickPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            bShouldStartDrag = (LocalClickPos.Y >= 0 && LocalClickPos.Y <= 40.0f);
            
            UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: No DragHandle found, using title area - LocalClick: (%f, %f), ShouldDrag: %s"), 
                LocalClickPos.X, LocalClickPos.Y, bShouldStartDrag ? TEXT("true") : TEXT("false"));
        }

        if (bShouldStartDrag)
        {
            const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
            const FVector2D Screen = InMouseEvent.GetScreenSpacePosition();
            const FVector2D MouseVP = Screen / Scale;
            
            DragOffset = MouseVP - CurrentViewportPosition;
            bDragging = true;

            UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Starting drag - MouseVP: (%f, %f), CurrentPos: (%f, %f), Offset: (%f, %f)"), 
                MouseVP.X, MouseVP.Y, CurrentViewportPosition.X, CurrentViewportPosition.Y, DragOffset.X, DragOffset.Y);

            if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            {
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            }
            return FReply::Handled();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Drag not started - mouse click outside drag area"));
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
    Pos.X = FMath::Clamp(Pos.X, DragPadding.Left, FMath::Max(DragPadding.Left, ViewportSize.X - Size.X - DragPadding.Right));
    Pos.Y = FMath::Clamp(Pos.Y, DragPadding.Top, FMath::Max(DragPadding.Top, ViewportSize.Y - Size.Y - DragPadding.Bottom));

    // Update position
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}

void UAvailableSkillsWidget::SkillInitialize(UMyGameInstance* InGameInstance)
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
        
        UE_LOG(LogTemp, Warning, TEXT("AvailableSkillsWidget: Widget shown - Skills: %d"), SkillItemWidgets.Num());
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
    
    // Уведомляем UIManager об изменении видимости
    if (OnWidgetVisibilityChanged.IsBound())
    {
        OnWidgetVisibilityChanged.Broadcast(false);
    }
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

    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Skill drag started — panel is now HitTestInvisible"));
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

    UE_LOG(LogTemp, Log, TEXT("AvailableSkillsWidget: Skill drag ended — panel restored"));
}
