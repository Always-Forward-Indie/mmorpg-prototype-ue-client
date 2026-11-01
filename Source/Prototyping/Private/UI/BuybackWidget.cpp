#include "UI/BuybackWidget.h"
#include "Prototyping.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UBuybackRowBinding::Setup(UBuybackWidget* InWidget, int32 InSlot, int32 InQuantity)
{
	Widget = InWidget;
	BoundSlot = InSlot;
	Quantity = InQuantity;
}

void UBuybackRowBinding::HandleClicked()
{
	// Full-stack repurchase (v1: no partial quantities).
	if (Widget)
		Widget->OnBuybackRowClicked.Broadcast(BoundSlot, Quantity);
}

void UBuybackWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Blueprint layout wins when bound; otherwise build the code fallback.
	// Widget-level placement stays in code (viewport anchors are runtime-only).
	if (!bWired)
	{
		bWired = true;
		if (Buyback_Title_Text || Buyback_List_Box || Buyback_Close_Btn)
		{
			HeaderText = Buyback_Title_Text;
			ListBox = Buyback_List_Box;
			if (Buyback_Close_Btn)
				Buyback_Close_Btn->OnClicked.AddDynamic(this, &UBuybackWidget::HideBuyback);
		}
		else
		{
			BuildLayout(); // wires its own close button
		}
	}
	SetAnchorsInViewport(FAnchors(0.f, 0.5f));
	SetAlignmentInViewport(FVector2D(0.f, 0.5f));
	SetPositionInViewport(WindowPosition, false);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBuybackWidget::NativeDestruct()
{
	RowBindings.Reset();
	Super::NativeDestruct();
}

void UBuybackWidget::BuildLayout()
{
	if (RootCol) return;
	UWidgetTree* Tree = WidgetTree;
	if (!Tree) return;

	USizeBox* Root = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Root->SetWidthOverride(WindowSize.X);
	Root->SetHeightOverride(WindowSize.Y);
	Tree->RootWidget = Root;

	RootCol = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Root->AddChild(RootCol);

	HeaderText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HeaderText->SetText(FText::FromString(TEXT("Buyback")));
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f)));
	HeaderText->SetShadowColorAndOpacity(FLinearColor::Black);
	HeaderText->SetShadowOffset(FVector2D(1.f, 1.f));
	if (UVerticalBoxSlot* Head = Cast<UVerticalBoxSlot>(RootCol->AddChildToVerticalBox(HeaderText)))
		Head->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	ListBox = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	if (UVerticalBoxSlot* Fill = Cast<UVerticalBoxSlot>(RootCol->AddChildToVerticalBox(ListBox)))
		Fill->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* CloseBtn = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* CloseTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloseTxt->SetText(FText::FromString(TEXT("Close")));
	CloseBtn->AddChild(CloseTxt);
	CloseBtn->OnClicked.AddDynamic(this, &UBuybackWidget::HideBuyback);
	RootCol->AddChildToVerticalBox(CloseBtn);
}

void UBuybackWidget::ShowBuyback(const TArray<FBuybackEntry>& Entries)
{
	ClearList();
	if (Entries.Num() == 0 && ListBox)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("Nothing to buy back")));
		Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
		ListBox->AddChild(Empty);
	}
	for (const FBuybackEntry& Entry : Entries)
		AddEntryRow(Entry, LocalizeItem(Entry.slug));
	SetVisibility(ESlateVisibility::Visible);
}

void UBuybackWidget::HideBuyback()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UBuybackWidget::IsBuybackOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UBuybackWidget::ClearList()
{
	if (ListBox)
		ListBox->ClearChildren();
	RowBindings.Reset();
}

void UBuybackWidget::AddEntryRow(const FBuybackEntry& Entry, const FString& DisplayName)
{
	if (!ListBox || !WidgetTree) return;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const FString Label = FString::Printf(TEXT("%s x%d — %d g"), *DisplayName, Entry.quantity, Entry.totalPrice);
	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Txt->SetText(FText::FromString(Label));
	Txt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Txt->SetShadowColorAndOpacity(FLinearColor::Black);
	Txt->SetShadowOffset(FVector2D(1.f, 1.f));
	Txt->SetAutoWrapText(true);
	Row->AddChildToHorizontalBox(Txt);

	UButton* BuyBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* BuyTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BuyTxt->SetText(FText::FromString(TEXT("Buy")));
	BuyBtn->AddChild(BuyTxt);
	UBuybackRowBinding* Binding = NewObject<UBuybackRowBinding>(this);
	Binding->Setup(this, Entry.slot, Entry.quantity);
	RowBindings.Add(Binding);
	BuyBtn->OnClicked.AddDynamic(Binding, &UBuybackRowBinding::HandleClicked);
	if (UHorizontalBoxSlot* BtnSlot = Cast<UHorizontalBoxSlot>(Row->AddChildToHorizontalBox(BuyBtn)))
		BtnSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

	ListBox->AddChild(Row);
}

FString UBuybackWidget::LocalizeItem(const FString& Slug) const
{
	if (Slug.IsEmpty()) return TEXT("—");
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
		{
			const FText LocName = Loc->GetItemDisplayName(Slug);
			if (!LocName.IsEmpty())
				return LocName.ToString();
		}
	}
	return Slug;
}
