#include "UI/ItemActionRowWidget.h"
#include "Components/Button.h"
#include "Styling/SlateBrush.h"
#include "Blueprint/WidgetTree.h"

void UItemActionRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	WireButton();
}

void UItemActionRowWidget::WireButton()
{
	if (RowButton)
		RowButton->OnClicked.AddDynamic(this, &UItemActionRowWidget::HandleRowButtonClicked);
}

void UItemActionRowWidget::HandleRowButtonClicked()
{
	OnRowClicked.Broadcast(BoundAction);
}

void UItemActionRowWidget::BuildCodeRow(const FText& Label)
{
	// Flat button matching the old Border look (dark normal, lighter hover):
	// same pixels, now with native press/focus/click sounds.
	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Flat;
	Flat.Normal = FSlateColorBrush(NormalBackgroundColor);
	Flat.Hovered = FSlateColorBrush(HoverBackgroundColor);
	Flat.Pressed = FSlateColorBrush(FLinearColor(0.12f, 0.12f, 0.12f, 0.95f));
	Flat.NormalPadding = FMargin(12.f, 6.f);
	Flat.PressedPadding = FMargin(12.f, 6.f);
	Btn->SetStyle(Flat);

	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Txt->SetText(Label);
	Txt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Btn->SetContent(Txt);

	WidgetTree->RootWidget = Btn;
	RowButton = Btn;
	WireButton();
}

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
	// Button rows own their hover visuals (style); tinting the background too
	// would double-darken. Visual-only rows keep the manual tint.
	if (RowBackground && !RowButton)
	{
		RowBackground->SetBrushColor(HoverBackgroundColor);
	}
}

void UItemActionRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (RowBackground && !RowButton)
	{
		RowBackground->SetBrushColor(NormalBackgroundColor);
	}
}
