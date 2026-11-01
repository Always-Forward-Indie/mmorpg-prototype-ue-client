#include "UI/PlayerMenuRowWidget.h"
#include "Prototyping.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void UPlayerMenuRowWidget::SetActionLabel(const FText& InLabel)
{
	if (MenuRow_Label)
		MenuRow_Label->SetText(InLabel);
}

bool UPlayerMenuRowWidget::IsPointInside(const FVector2D& AbsoluteScreenPos) const
{
	if (!GetCachedWidget().IsValid()) return false;
	const FGeometry Geo = GetCachedGeometry();
	const FVector2D Local = Geo.AbsoluteToLocal(AbsoluteScreenPos);
	const FVector2D Size = Geo.GetLocalSize();
	return Local.X >= 0.f && Local.X <= Size.X && Local.Y >= 0.f && Local.Y <= Size.Y;
}

void UPlayerMenuRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (RowBackground)
		RowBackground->SetBrushColor(HoverBackgroundColor);
}

void UPlayerMenuRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (RowBackground)
		RowBackground->SetBrushColor(NormalBackgroundColor);
}
