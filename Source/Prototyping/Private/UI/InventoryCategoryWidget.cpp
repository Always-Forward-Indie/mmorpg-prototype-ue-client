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

void UCategoryRowWidget::SetupRow(const FText& Label, bool bChecked)
{
	if (RowLabel) RowLabel->SetText(Label);
	if (RowCheckBox) RowCheckBox->SetIsChecked(bChecked);
}

static void MakeCodeRow(UWidgetTree* Tree, UVerticalBox* List,
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
}

void UInventoryCategoryWidget::AddRow(const FText& Label, bool bInitial, UCheckBox*& OutBox)
{
	// BP row design wins when the class is assigned and the list exists.
	if (CategoryRowWidgetClass && CategoryList)
	{
		if (UCategoryRowWidget* Row = CreateWidget<UCategoryRowWidget>(this, CategoryRowWidgetClass))
		{
			Row->SetupRow(Label, bInitial);
			CategoryList->AddChild(Row);
			OutBox = Row->GetCheckBox();
			if (!OutBox)
			{
				UE_LOG(LogGameplay, Error, TEXT("InventoryCategoryWidget: CategoryRowWidgetClass row has no 'RowCheckBox' bind — row is dead."));
			}
			return;
		}
		UE_LOG(LogGameplay, Error, TEXT("InventoryCategoryWidget: failed to create category row — falling back to code-built row."));
	}
	MakeCodeRow(WidgetTree, CategoryList ? CategoryList : DefaultList, OutBox, Label, bInitial);
}

void UInventoryCategoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CategoryList)
	{
		// BP panel: rows land in the bound list; tint the bound frame.
		if (CategoryFrame)
		{
			CategoryFrame->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.96f));
			CategoryFrame->SetPadding(FMargin(4.f));
		}
	}
	else
	{
		// Code-built default: dark panel + list (zero BP setup).
		UBorder* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Root->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.96f));
		Root->SetPadding(FMargin(4.f));
		DefaultList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Root->SetContent(DefaultList);
		WidgetTree->RootWidget = Root;
	}

	AddRow(LOCTEXT("CatAll", "All"), true, AllBox);
	AddRow(LOCTEXT("CatEquipment", "Equipment"), true, EquipmentBox);
	AddRow(LOCTEXT("CatUsable", "Usable"), true, UsableBox);
	AddRow(LOCTEXT("CatQuest", "Quest"), true, QuestBox);
	AddRow(LOCTEXT("CatOther", "Other"), true, OtherBox);

	// A BP row class without its checkbox bind is fatal for that row —
	// AddRow already Errored; never bind a null box.
	if (AllBox) AllBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnAllChecked);
	if (EquipmentBox) EquipmentBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnEquipmentChecked);
	if (UsableBox) UsableBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnUsableChecked);
	if (QuestBox) QuestBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnQuestChecked);
	if (OtherBox) OtherBox->OnCheckStateChanged.AddDynamic(this, &UInventoryCategoryWidget::OnOtherChecked);

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
	if (AllBox) AllBox->SetIsChecked(bEquip && bUsable && bQuest && bOther);
	if (EquipmentBox) EquipmentBox->SetIsChecked(bEquip);
	if (UsableBox) UsableBox->SetIsChecked(bUsable);
	if (QuestBox) QuestBox->SetIsChecked(bQuest);
	if (OtherBox) OtherBox->SetIsChecked(bOther);
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
