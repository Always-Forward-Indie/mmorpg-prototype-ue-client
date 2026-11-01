#include "UI/KeybindWidget.h"
#include "Prototyping.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Input/KeybindManager.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "InputMappingContext.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"

void UKeybindWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildLayout();
	SetAnchorsInViewport(FAnchors(0.5f, 0.5f));
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	SetPositionInViewport(WindowPosition, false);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UKeybindWidget::NativeDestruct()
{
	StopCapture();
	Super::NativeDestruct();
}

void UKeybindWidget::BuildLayout()
{
	if (RootCol) return;
	UWidgetTree* Tree = WidgetTree;
	if (!Tree) return;

	USizeBox* Root = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Root->SetWidthOverride(WindowSize.X);
	Root->SetHeightOverride(WindowSize.Y);
	Tree->RootWidget = Root;

	UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.03f, 0.97f));
	Frame->SetPadding(FMargin(14.f, 10.f));
	Root->AddChild(Frame);

	RootCol = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->AddChild(RootCol);

	UHorizontalBox* HeadRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RootCol->AddChildToVerticalBox(HeadRow);

	UTextBlock* Title = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(FText::FromString(TEXT("Controls — click a key, then press a new one")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f)));
	if (UHorizontalBoxSlot* TitleSlot = Cast<UHorizontalBoxSlot>(HeadRow->AddChildToHorizontalBox(Title)))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	UButton* ResetBtn = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* ResetTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTxt->SetText(FText::FromString(TEXT("Reset all")));
	ResetBtn->AddChild(ResetTxt);
	ResetBtn->OnClicked.AddDynamic(this, &UKeybindWidget::HandleResetAllClicked);
	HeadRow->AddChildToHorizontalBox(ResetBtn);

	ListBox = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	if (UVerticalBoxSlot* Fill = Cast<UVerticalBoxSlot>(RootCol->AddChildToVerticalBox(ListBox)))
		Fill->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* CloseBtn = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* CloseTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloseTxt->SetText(FText::FromString(TEXT("Close")));
	CloseBtn->AddChild(CloseTxt);
	CloseBtn->OnClicked.AddDynamic(this, &UKeybindWidget::HandleCloseClicked);
	RootCol->AddChildToVerticalBox(CloseBtn);
}

void UKeybindWidget::BindToPlayer(ABasicPlayer* InPlayer, UKeybindManager* InManager)
{
	Player = InPlayer;
	KeybindManager = InManager;
	MappingContext = nullptr;
	if (Player)
		MappingContext = Player->GetInputMappingContext();
	RefreshList();
}

void UKeybindWidget::RefreshList()
{
	ClearRows();
	if (!Player || !KeybindManager || !MappingContext || !ListBox) return;
	for (const FKeybindActionEntry& Entry : KeybindManager->CollectActions(Player))
		AddActionRow(Entry, KeybindManager->GetPrimaryKey(MappingContext, Entry.Action));
}

void UKeybindWidget::CloseKeybinds()
{
	StopCapture();
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UKeybindWidget::IsKeybindsOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UKeybindWidget::ClearRows()
{
	if (ListBox)
		ListBox->ClearChildren();
	RowBindings.Reset();
}

void UKeybindWidget::AddActionRow(const FKeybindActionEntry& Entry, const FKey& BoundKey)
{
	if (!ListBox || !WidgetTree) return;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(Entry.Label);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(Row->AddChildToHorizontalBox(Label)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	UButton* KeyBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* KeyTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	const bool bCapturing = (CapturingAction == Entry.Action);
	KeyTxt->SetText(FText::FromString(bCapturing ? TEXT("press a key… (Esc cancels)")
		: BoundKey.IsValid() ? BoundKey.ToString() : TEXT("—")));
	KeyBtn->AddChild(KeyTxt);
	UKeybindRowBinding* Binding = NewObject<UKeybindRowBinding>(this);
	Binding->Setup(this, Entry.Action);
	RowBindings.Add(Binding);
	KeyBtn->OnClicked.AddDynamic(Binding, &UKeybindRowBinding::HandleClicked);
	if (UHorizontalBoxSlot* KeySlot = Cast<UHorizontalBoxSlot>(Row->AddChildToHorizontalBox(KeyBtn)))
		KeySlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

	ListBox->AddChild(Row);
}

void UKeybindRowBinding::Setup(UKeybindWidget* InWidget, UInputAction* InAction)
{
	Widget = InWidget;
	Action = InAction;
}

void UKeybindRowBinding::HandleClicked()
{
	if (Widget)
		Widget->RequestCapture(Action);
}

void UKeybindWidget::RequestCapture(UInputAction* Action)
{
	if (!Action || !MappingContext) return;
	StopCapture();
	CapturingAction = Action;
	RefreshList();
	if (FSlateApplication::IsInitialized())
	{
		CaptureHandle = FSlateApplication::Get().OnApplicationPreInputKeyDownListener()
			.AddUObject(this, &UKeybindWidget::OnCaptureKeyDown);
	}
}

void UKeybindWidget::StopCapture()
{
	CapturingAction = nullptr;
	if (CaptureHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(CaptureHandle);
		CaptureHandle.Reset();
	}
}

void UKeybindWidget::OnCaptureKeyDown(const FKeyEvent& InKeyEvent)
{
	if (!CapturingAction || !KeybindManager || !MappingContext) return;
	const FKey Pressed = InKeyEvent.GetKey();
	if (Pressed == EKeys::Escape)
	{
		StopCapture();
		RefreshList();
		return;
	}
	KeybindManager->RemapAction(MappingContext, CapturingAction, Pressed);
	StopCapture();
	RefreshList();
}

void UKeybindWidget::HandleResetAllClicked()
{
	if (KeybindManager && MappingContext)
		KeybindManager->ResetAll(MappingContext);
	RefreshList();
}

void UKeybindWidget::HandleCloseClicked()
{
	CloseKeybinds();
}
