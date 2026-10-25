#include "UI/InventoryWidget.h"
#include "Prototyping.h"
#include "UI/InventorySlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "UI/ItemActionMenuWidget.h"
#include "UI/DropQuantityPopupWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Components/GridPanel.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsInventoryVisible && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Close context menu when clicking outside its action rows
		CloseContextMenu();

		// Hide tooltip when clicking anywhere in the inventory window
		HideTooltip();
		
		bool bShouldStartDrag = false;

		if (DragHandle)
		{
			// ����������������� DragHandle
			const FGeometry DragHandleGeometry = DragHandle->GetCachedGeometry();
			const FVector2D LocalMousePos = DragHandleGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

			// ���������, ����� �� ���� � ������� DragHandle
			const FVector2D DragHandleSize = DragHandleGeometry.GetLocalSize();
			bShouldStartDrag = (LocalMousePos.X >= 0 && LocalMousePos.X <= DragHandleSize.X &&
				LocalMousePos.Y >= 0 && LocalMousePos.Y <= DragHandleSize.Y);

			UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: DragHandle click test - LocalPos: %s, Size: %s, ShouldDrag: %s"),
				*LocalMousePos.ToString(), *DragHandleSize.ToString(), bShouldStartDrag ? TEXT("True") : TEXT("False"));
		}
		else
		{
			// ���� ��� DragHandle, ��������� �������������� �����
			bShouldStartDrag = true;
			UE_LOG(LogGameplay, Warning, TEXT("InventoryWidget: No DragHandle, allowing drag from anywhere"));
		}

		if (bShouldStartDrag)
		{
			const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);

			const FVector2D Screen = InMouseEvent.GetScreenSpacePosition();
			const FVector2D MouseVP = Screen / Scale;

			// �����! ���������� ������� ������� �������, � �� ��������� �� �������
			const FVector2D CurrentPos = CurrentViewportPosition; // ���������� ������������� �������

			DragOffset = MouseVP - CurrentPos;
			bDragging = true;

			UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Drag calculation - Mouse: %s, Current: %s, Offset: %s"),
				*MouseVP.ToString(), *CurrentPos.ToString(), *DragOffset.ToString());

			if (TSharedPtr<SWidget> Slate = GetCachedWidget())
			{
				return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
			}
			return FReply::Handled();
		}
		else
		{
			UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Click outside DragHandle area - drag not started"));
		}
	}
	
	// Hide tooltip on right mouse button click as well
	if (bIsInventoryVisible && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		HideTooltip();
	}
	
	return FReply::Unhandled();
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;

		if (TSharedPtr<SWidget> Slate = (DragHandle ? DragHandle->GetCachedWidget() : GetCachedWidget()))
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Mouse move during drag - Position: %s"),
			*InMouseEvent.GetScreenSpacePosition().ToString());
		UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UInventoryWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPosAbs)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryWidget: No PlayerController found for drag"));
		return;
	}

	int32 W = 0, H = 0;
	PC->GetViewportSize(W, H);

	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	const FVector2D ViewportSize = FVector2D(W, H) / Scale;

	FVector2D Size = GetDesiredSize();
	if (Size.IsZero()) Size = FVector2D(400, 300);

	// ������ -> viewport space
	const FVector2D MouseVP = ScreenCursorPosAbs / Scale;

	// �������� ������� � viewport space
	FVector2D Pos = MouseVP - DragOffset;

	// ������
	Pos.X = FMath::Clamp(Pos.X, DragPadding.Left, FMath::Max(DragPadding.Left, ViewportSize.X - Size.X - DragPadding.Right));
	Pos.Y = FMath::Clamp(Pos.Y, DragPadding.Top, FMath::Max(DragPadding.Top, ViewportSize.Y - Size.Y - DragPadding.Bottom));

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Previous position: %s, New position: %s"),
		*CurrentViewportPosition.ToString(), *Pos.ToString());

	// Update our tracked position
	CurrentViewportPosition = Pos;
	SetPositionInViewport(Pos, false);
}

