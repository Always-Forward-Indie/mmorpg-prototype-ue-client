#include "UI/FocusableWindowWidget.h"

FReply UFocusableWindowWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	OnWindowFocusRequested.Broadcast(this);
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}
