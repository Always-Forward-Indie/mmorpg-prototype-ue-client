#include "UI/PlayerContextMenuWidget.h"
#include "Prototyping.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetLayoutLibrary.h"

#define LOCTEXT_NAMESPACE "PlayerMenu"

UPlayerContextMenuWidget::UPlayerContextMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPlayerContextMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

FReply UPlayerContextMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		PressedRowIndex = HitTestRowIndex(InMouseEvent.GetScreenSpacePosition());
		if (TSharedPtr<SWidget> Slate = GetCachedWidget())
			return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
		return FReply::Handled();
	 }
	// Consume RMB and the rest — the release that opened us must not propagate.
	return FReply::Handled();
}

FReply UPlayerContextMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 UpRow = HitTestRowIndex(InMouseEvent.GetScreenSpacePosition());
		const int32 DownRow = PressedRowIndex;
		PressedRowIndex = -1;
		if (DownRow >= 0 && DownRow == UpRow && Rows.IsValidIndex(DownRow))
		{
			const EPlayerSocialAction Action = Rows[DownRow].Action;
			const int32 Target = TargetCharacterId;
			HideMenu();
			OnActionSelected.Broadcast(Action, Target);
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UPlayerContextMenuWidget::ShowForPlayer(int32 InCharacterId, const FString& DisplayName, FVector2D ScreenPosition)
{
	TargetCharacterId = InCharacterId;
	PressedRowIndex = -1;
	RebuildRows(DisplayName, InCharacterId > 0);

	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	SetPositionInViewport(ScreenPosition / Scale, false);
	SetVisibility(ESlateVisibility::Visible);
}

void UPlayerContextMenuWidget::HideMenu()
{
	PressedRowIndex = -1;
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UPlayerContextMenuWidget::IsMenuOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UPlayerContextMenuWidget::RebuildRows(const FString& DisplayName, bool bCanTrade)
{
	Rows.Reset();

	UWidgetTree* Tree = WidgetTree;
	if (!Tree) return;

	UBorder* Root = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Root->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.04f, 0.95f));
	Root->SetPadding(FMargin(8.f, 6.f));
	Tree->RootWidget = Root;

	UVerticalBox* List = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Root->AddChild(List);

	UTextBlock* Title = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(DisplayName));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f)));
	if (UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(List->AddChildToVerticalBox(Title)))
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	AddRow(EPlayerSocialAction::Whisper, LOCTEXT("Whisper", "Whisper"));
	if (bCanTrade)
		AddRow(EPlayerSocialAction::Trade, LOCTEXT("Trade", "Trade"));
	AddRow(EPlayerSocialAction::Inspect, LOCTEXT("Inspect", "Inspect"));

	// Attach rows to the list (AddRow only records; widgets parent here).
	for (const FMenuRow& Row : Rows)
	{
		if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(List->AddChildToVerticalBox(Row.Widget)))
		{
			RowSlot->SetPadding(FMargin(0.f, 1.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UPlayerContextMenuWidget::AddRow(EPlayerSocialAction Action, const FText& Label)
{
	if (!WidgetTree) return;
	UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Bg->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.9f));
	Bg->SetPadding(FMargin(12.f, 6.f));
	Bg->SetHorizontalAlignment(HAlign_Fill);

	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Txt->SetText(Label);
	Txt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Bg->AddChild(Txt);

	FMenuRow Row;
	Row.Action = Action;
	Row.Widget = Bg;
	Rows.Add(Row);
}

int32 UPlayerContextMenuWidget::HitTestRowIndex(const FVector2D& AbsoluteScreenPos) const
{
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		UWidget* Child = Rows[i].Widget;
		if (!Child) continue;
		const FGeometry Geo = Child->GetCachedGeometry();
		const FVector2D Local = Geo.AbsoluteToLocal(AbsoluteScreenPos);
		const FVector2D Size = Geo.GetLocalSize();
		if (Local.X >= 0.f && Local.X <= Size.X && Local.Y >= 0.f && Local.Y <= Size.Y)
			return i;
	}
	return -1;
}

#undef LOCTEXT_NAMESPACE
