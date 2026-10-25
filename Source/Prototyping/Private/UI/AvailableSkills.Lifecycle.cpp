#include "UI/AvailableSkillsWidget.h"
#include "MyGameInstance.h"
#include "Prototyping.h"
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

// S5: widget chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "AvailableSkills"

void UAvailableSkillsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    bIsVisible = false;
    bIsShowingTooltip = false;
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
            UE_LOG(LogGameplay, Verbose, TEXT("AvailableSkillsWidget: Created skill tooltip widget"));
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
        TextBlock_Title->SetText(LOCTEXT("Skills", "Skills"));
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

#undef LOCTEXT_NAMESPACE

