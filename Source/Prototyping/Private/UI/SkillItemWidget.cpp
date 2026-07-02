#include "UI/SkillItemWidget.h"
#include "UI/SkillDragDropOperation.h"
#include "UI/SkillDragVisualWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Input/Events.h"

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
        
        // DetectDrag ������������� ������� NativeOnDragDetected ��� ���������� ������
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }
    
    return FReply::Unhandled();
}

FReply USkillItemWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // ������ ���������� ������������� ������
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
        else
        {
            // Drag ended via mouse release (successful drop or aborted).
            // NativeOnDragCancelled covers the cancelled case; here we handle the
            // "mouse released after drag" path that UE routes differently.
            OnSkillItemDragEnded.Broadcast(CurrentSkillData);
        }

        bIsDragging = false;
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

// ���������� ���������� NativeOnDragDetected
void USkillItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL DRAG DETECTED DEBUG START ==="));
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: DRAG DETECTED for skill %s"), *CurrentSkillData.networkData.skillSlug);

    // Hide tooltip when starting drag operation
    OnSkillItemHovered.Broadcast(CurrentSkillData, false);

    // ������� DragDropOperation
    USkillDragDropOperation* DragDropOp = NewObject<USkillDragDropOperation>(this, USkillDragDropOperation::StaticClass());
    if (!DragDropOp)
    {
        UE_LOG(LogTemp, Error, TEXT("SkillItemWidget: ? Failed to create DragDropOperation"));
        OutOperation = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("=== SKILL DRAG DETECTED DEBUG END (FAILED) ==="));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ? Created DragDropOperation successfully"));

    // ����������� ��������
    DragDropOp->SetSkillData(CurrentSkillData, this);
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ? Set skill data in DragDropOperation"));
    
    // �����: �������� ����� drag visual �� USkillItemWidget � DragDropOperation
    if (DragVisualWidgetClass)
    {
        DragDropOp->DragVisualWidgetClass = DragVisualWidgetClass;
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Set custom DragVisualWidgetClass: %s"), *DragVisualWidgetClass->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: No custom DragVisualWidgetClass set"));
    }
    
    // ������� drag visual
    UUserWidget* DragVisual = DragDropOp->CreateDragVisualWidget();
    if (DragVisual)
    {
        DragDropOp->DefaultDragVisual = DragVisual;
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ? Created custom drag visual"));
    }
    else
    {
        // Never use the source widget as the drag visual — it reparents the
        // source out of its container and corrupts layout/hit-testing for the
        // duration of the drag. Proceed without a visual instead.
        DragDropOp->DefaultDragVisual = nullptr;
        UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: CreateDragVisualWidget returned null — proceeding without drag visual"));
    }
    
    // ��������� �������
    DragDropOp->Pivot = EDragPivot::MouseDown;
    DragDropOp->Offset = FVector2D::ZeroVector;
    
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ? Configured drag operation settings"));
    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: ? Setting OutOperation and marking as dragging"));
    
    OutOperation = DragDropOp;
    bIsDragging = true;

    // Notify parent AvailableSkillsWidget so it can make itself HitTestInvisible,
    // allowing drag events to reach the SkillBar below it.
    OnSkillItemDragStarted.Broadcast(CurrentSkillData);

    UE_LOG(LogTemp, Warning, TEXT("SkillItemWidget: Started dragging skill %s"), *CurrentSkillData.networkData.skillSlug);
    UE_LOG(LogTemp, Warning, TEXT("=== SKILL DRAG DETECTED DEBUG END (SUCCESS) ==="));
}

void USkillItemWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

    // Always broadcast drag-ended and reset state. The bIsDragging guard was
    // previously unreliable because NativeOnMouseLeave could clear it early.
    bIsDragging = false;
    OnSkillItemDragEnded.Broadcast(CurrentSkillData);
    UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: Drag cancelled for skill %s"), *CurrentSkillData.networkData.skillSlug);
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
    // Do NOT reset bIsDragging here. During an active UE drag-drop operation the
    // cursor leaves the source widget, which fires NativeOnMouseLeave. Clearing
    // bIsDragging would break NativeOnDragCancelled and NativeOnMouseButtonUp,
    // preventing OnSkillItemDragEnded from ever being broadcast.
    UpdateHoverState();

    // Broadcast hover event
    OnSkillItemHovered.Broadcast(CurrentSkillData, false);

    UE_LOG(LogTemp, Log, TEXT("SkillItemWidget: Mouse left skill %s (dragging: %s)"),
        *CurrentSkillData.networkData.skillSlug, bIsDragging ? TEXT("true") : TEXT("false"));
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
        FString LevelText = FString::Printf(TEXT("lvl %d"), CurrentSkillData.networkData.skillLevel);
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