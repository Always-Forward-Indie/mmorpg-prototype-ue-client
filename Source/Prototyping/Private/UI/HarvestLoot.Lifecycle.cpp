#include "UI/HarvestLootWidget.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "UI/HarvestLootItemWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "Engine/Engine.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

void UHarvestLootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsVisible = false;
	CurrentLootItems.Empty();
	LootItemWidgets.Empty();
	HarvestManager = nullptr;
	HoveredItemId = -1;

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

	// Initialize widget but don't bind to HarvestManager yet
	InitializeWidget();

	// Bind button events
	if (Button_PickupAll)
	{
		Button_PickupAll->OnClicked.AddDynamic(this, &UHarvestLootWidget::OnPickupAllClicked);
	}

	if (Button_Close)
	{
		Button_Close->OnClicked.AddDynamic(this, &UHarvestLootWidget::OnCloseClicked);
	}

	// Create tooltip widget if class is set
	if (ItemTooltipWidgetClass)
	{
		ItemTooltipWidget = CreateWidget<UItemTooltipWidget>(this, ItemTooltipWidgetClass);
		if (ItemTooltipWidget)
		{
			ItemTooltipWidget->AddToViewport(1000); // High Z-order for tooltip
			ItemTooltipWidget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UHarvestLootWidget::NativeDestruct()
{
	// Clean up tooltip
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->RemoveFromParent();
		ItemTooltipWidget = nullptr;
	}

	UnbindFromHarvestManager();
	Super::NativeDestruct();
}

void UHarvestLootWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update tooltip position if visible
	if (bIsVisible && ItemTooltipWidget && ItemTooltipWidget->GetVisibility() == ESlateVisibility::HitTestInvisible)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			FVector2D Mouse;
			if (PC->GetMousePosition(Mouse.X, Mouse.Y))
			{
				ItemTooltipWidget->ForceLayoutPrepass();
				ItemTooltipWidget->UpdateTooltipPosition(Mouse);
			}
		}
	}
}

FReply UHarvestLootWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsVisible && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Hide tooltip when clicking anywhere in the harvest loot window
		HideTooltip();
		
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
		HideTooltip();
	}
	
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UHarvestLootWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

FReply UHarvestLootWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UHarvestLootWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPosAbs)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);

	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	const FVector2D ViewportSize = FVector2D(W, H) / Scale;

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

