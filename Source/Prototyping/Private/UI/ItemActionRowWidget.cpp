#include "UI/ItemActionRowWidget.h"

void UItemActionRowWidget::SetActionLabel(const FText& InLabel)
{
	if (ActionLabel)
	{
		ActionLabel->SetText(InLabel);
	}
}

void UItemActionRowWidget::SetActionIcon(UTexture2D* InIcon)
{
	if (ActionIcon)
	{
		ActionIcon->SetBrushFromTexture(InIcon);
	}
}

bool UItemActionRowWidget::IsPointInside(const FVector2D& AbsoluteScreenPos) const
{
	const FGeometry Geo = GetCachedGeometry();
	const FVector2D Local = Geo.AbsoluteToLocal(AbsoluteScreenPos);
	const FVector2D Size  = Geo.GetLocalSize();
	return Local.X >= 0.f && Local.X <= Size.X && Local.Y >= 0.f && Local.Y <= Size.Y;
}

void UItemActionRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (RowBackground)
	{
		RowBackground->SetBrushColor(HoverBackgroundColor);
	}
}

void UItemActionRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (RowBackground)
	{
		RowBackground->SetBrushColor(NormalBackgroundColor);
	}
}
