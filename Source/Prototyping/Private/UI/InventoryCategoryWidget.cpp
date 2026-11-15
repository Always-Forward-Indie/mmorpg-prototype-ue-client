#include "UI/InventoryCategoryWidget.h"
#include "Prototyping.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"

#define LOCTEXT_NAMESPACE "InventoryCategory"

UInventoryCategoryWidget::UInventoryCategoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static UHorizontalBox* MakeCheckRow(UWidgetTree* Tree, UVerticalBox* List,
	UCheckBox*& OutBox, const FText& Label, bool bInitial)
{
	UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UCheckBox* Box = Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	Box->SetIsChecked(bInitial);
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(Row->AddChild(Box)))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
	if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(Row->AddChild(Text)))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(List->AddChild(Row)))
	{
		S->SetPadding(FMargin(8.f, 4.f));
		S->SetHorizontalAlignment(HAlign_Left);
	}
	OutBox = Box;
	return Row;
}

void UInventoryCategoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Code-built panel: dark border + check rows (zero BP setup).
	UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Root->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.96f));
	Root->SetPadding(FMargin(4.f));
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Root->SetContent(List);
	WidgetTree->RootWidget = Root;

	MakeCheckRow(WidgetTree, List, AllBox, LOCTEXT("CatAll", "All"), true);
	MakeCheckRow(WidgetTree, List, EquipmentBox, LOCTEXT("CatEquipment", "Equipment"), true);
	MakeCheckRow(WidgetTree, List, UsableBox, LOCTEXT("CatUsable", "Usable"), true);
	MakeCheckRow(WidgetTree, List, QuestBox, LOCTEXT("CatQuest", "Quest"), true);
	MakeCheckRow(WidgetTree, List, OtherBox, LOCTEXT("CatOther", "Other"), true);

	AllBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnAllChecked);
	EquipmentBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnEquipmentChecked);
	UsableBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnUsableChecked);
	QuestBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnQuestChecked);
	OtherBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnOtherChecked);

	SetVisibility(ESlateVisibility::Collapsed);
}

void UInventoryCategoryWidget::ShowAt(const FVector2D& ScreenViewportPos)
{
	SetPositionInViewport(ScreenViewportPos, false);
	SetVisibility(ESlateVisibility::Visible);
}

void UInventoryCategoryWidget::HideFilter()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UInventoryCategoryWidget::IsFilterOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UInventoryCategoryWidget::GetActive(bool& bOutEquipment, bool& bOutUsable, bool& bOutQuest, bool& bOutOther) const
{
	bOutEquipment = bEquip;
	bOutUsable = bUsable;
	bOutQuest = bQuest;
	bOutOther = bOther;
}

FText UInventoryCategoryWidget::DisplayName(int32 BucketIndex)
{
	switch (BucketIndex)
	{
	case 0: return LOCTEXT("CatEquipment", "Equipment");
	case 1: return LOCTEXT("CatUsable", "Usable");
	case 2: return LOCTEXT("CatQuest", "Quest");
	default: return LOCTEXT("CatOther", "Other");
	}
}

FText UInventoryCategoryWidget::GetSummaryText() const
{
	const bool Flags[] = { bEquip, bUsable, bQuest, bOther };
	int32 Count = 0;
	for (bool b : Flags) if (b) ++Count;
	if (Count == 4 || Count == 0) return LOCTEXT("CatAll", "All");
	if (Count <= 2)
	{
		FString Joined;
		for (int32 i = 0; i < 4; ++i)
		{
			if (!Flags[i]) continue;
			if (!Joined.IsEmpty()) Joined += TEXT(" + ");
			Joined += DisplayName(i).ToString();
		}
		return FText::FromString(Joined);
	}
	return FText::Format(LOCTEXT("CatCustom", "Custom ({0})"), FText::AsNumber(Count));
}

void UInventoryCategoryWidget::ApplyAndBroadcast()
{
	// SetIsChecked never rebroadcasts: no feedback loop by construction.
	AllBox->SetIsChecked(bEquip && bUsable && bQuest && bOther);
	EquipmentBox->SetIsChecked(bEquip);
	UsableBox->SetIsChecked(bUsable);
	QuestBox->SetIsChecked(bQuest);
	OtherBox->SetIsChecked(bOther);
	OnSelectionChanged.Broadcast();
}

void UInventoryCategoryWidget::OnAllChecked(bool bChecked)
{
	if (bChecked)
	{
		bEquip = bUsable = bQuest = bOther = true;
	}
	else
	{
		// Unchecking All can never empty the grid: fall back to All.
		bEquip = bUsable = bQuest = bOther = true;
	}
	ApplyAndBroadcast();
}

void UInventoryCategoryWidget::OnEquipmentChecked(bool bChecked)
{
	bEquip = bChecked;
	if (!bEquip && !bUsable && !bQuest && !bOther)
		bEquip = bUsable = bQuest = bOther = true; // last one out -> All
	ApplyAndBroadcast();
}

void UInventoryCategoryWidget::OnUsableChecked(bool bChecked)
{
	bUsable = bChecked;
	if (!bEquip && !bUsable && !bQuest && !bOther)
		bEquip = bUsable = bQuest = bOther = true;
	ApplyAndBroadcast();
}

void UInventoryCategoryWidget::OnQuestChecked(bool bChecked)
{
	bQuest = bChecked;
	if (!bEquip && !bUsable && !bQuest && !bOther)
		bEquip = bUsable = bQuest = bOther = true;
	ApplyAndBroadcast();
}

void UInventoryCategoryWidget::OnOtherChecked(bool bChecked)
{
	bOther = bChecked;
	if (!bEquip && !bUsable && !bQuest && !bOther)
		bEquip = bUsable = bQuest = bOther = true;
	ApplyAndBroadcast();
}

#undef LOCTEXT_NAMESPACE
