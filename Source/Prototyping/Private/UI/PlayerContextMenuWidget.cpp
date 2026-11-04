#include "UI/PlayerContextMenuWidget.h"
#include "UI/PlayerMenuRowWidget.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
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

void UPlayerContextMenuWidget::FireAction(EPlayerSocialAction Action)
{
	const int32 Target = TargetCharacterId;
	HideMenu();
	OnActionSelected.Broadcast(Action, Target);
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

void UPlayerContextMenuWidget::ShowForPlayer(int32 InCharacterId, const FString& DisplayName, FVector2D ScreenPosition, bool bCanInspect, bool bCanTrade)
{
	TargetCharacterId = InCharacterId;
	PressedRowIndex = -1;

	if (Menu_Title_Text)
		Menu_Title_Text->SetText(FText::FromString(DisplayName));

	RebuildRows(DisplayName, bCanTrade && InCharacterId > 0, bCanInspect);
	UE_LOG(LogGameplay, Verbose, TEXT("PlayerMenu: ShowForPlayer target=%d rows=%d pos=(%.0f, %.0f) bpList=%d"),
		InCharacterId, Rows.Num(), ScreenPosition.X, ScreenPosition.Y, Menu_Action_List ? 1 : 0);

	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	SetPositionInViewport(ScreenPosition / Scale, false);
	// Instance wrapper must never eat hits (menu sits above HUD, at cursor).
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UPlayerContextMenuWidget::HideMenu()
{
	PressedRowIndex = -1;
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UPlayerContextMenuWidget::IsMenuOpen() const
{
	return IsVisible();
}

void UPlayerContextMenuWidget::RebuildRows(const FString& DisplayName, bool bCanTrade, bool bCanInspect)
{
	Rows.Reset();

	UWidgetTree* Tree = WidgetTree;
	if (!Tree) return;

	// Bound list wins; otherwise build the code fallback root.
	UVerticalBox* List = Menu_Action_List;
	if (!List)
	{
		UBorder* Root = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Root->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.04f, 0.95f));
		Root->SetPadding(FMargin(8.f, 6.f));
		// Transparent corners must not eat world clicks (menu sits above HUD).
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Tree->RootWidget = Root;

		List = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Root->AddChild(List);
	}
	else
	{
		List->ClearChildren();
	}

	// Title: bound TextBlock wins, otherwise a code row on top of the list.
	if (Menu_Title_Text)
	{
		Menu_Title_Text->SetText(FText::FromString(DisplayName));
	}
	else
	{
		UTextBlock* Title = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Title->SetText(FText::FromString(DisplayName));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f)));
		if (UVerticalBoxSlot* TitleSlot = Cast<UVerticalBoxSlot>(List->AddChildToVerticalBox(Title)))
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	AddRow(EPlayerSocialAction::Whisper, LOCTEXT("Whisper", "Whisper"), List);
	if (bCanTrade)
		AddRow(EPlayerSocialAction::Trade, LOCTEXT("Trade", "Trade"), List);
	if (bCanInspect)
		AddRow(EPlayerSocialAction::Inspect, LOCTEXT("Inspect", "Inspect"), List);

	// Global button sounds on freshly built rows (idempotent, null-safe).
	if (UMyGameInstance* AudioGI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (AudioGI->AudioManager && List)
		{
			AudioGI->AudioManager->ApplyButtonSoundsToSubtree(List);
		}
	}
}

void UPlayerContextMenuWidget::AddRow(EPlayerSocialAction Action, const FText& Label, UVerticalBox* List)
{
	if (!WidgetTree || !List) return;

	// Custom row widget from Blueprint
	if (MenuRowWidgetClass)
	{
		if (UPlayerMenuRowWidget* Row = CreateWidget<UPlayerMenuRowWidget>(this, MenuRowWidgetClass))
		{
			Row->SetActionLabel(Label);
			Row->SetBoundAction(Action);
			FMenuRow Rec;
			Rec.Action = Action;
			Rec.Widget = Row;
			Rows.Add(Rec);
			if (UVerticalBoxSlot* RowSlot = List->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(0.f, 1.f));
				RowSlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return;
	}

	// Fallback: programmatic Border > TextBlock
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

	if (UVerticalBoxSlot* RowSlot = List->AddChildToVerticalBox(Bg))
	{
		RowSlot->SetPadding(FMargin(0.f, 1.f));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}
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
