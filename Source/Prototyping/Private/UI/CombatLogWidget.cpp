#include "UI/CombatLogWidget.h"
#include "Prototyping.h"
#include "Gameplay/Combat/CombatLogManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"

void UCombatLogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Blueprint layout wins when bound — including placement: the BP root
	// carries its own anchors (full-screen root + arranged content is the
	// standard setup), so code must not override them. The code fallback
	// uses the LogSize/LogPosition/LogAnchors/LogAlignment tunables.
	if (!bWired)
	{
		bWired = true;
		if (Log_Scroll_Box)
		{
			LogScroll = Log_Scroll_Box;
			LogScroll->OnUserScrolled.AddDynamic(this, &UCombatLogWidget::HandleUserScrolled);
			if (Log_Filter_Row)
			{
				AddFilterCheck(ECombatLogCategory::Damage, TEXT("Dmg"), Log_Filter_Row);
				AddFilterCheck(ECombatLogCategory::Heal, TEXT("Heal"), Log_Filter_Row);
				AddFilterCheck(ECombatLogCategory::Loot, TEXT("Loot"), Log_Filter_Row);
				AddFilterCheck(ECombatLogCategory::Experience, TEXT("XP"), Log_Filter_Row);
				AddFilterCheck(ECombatLogCategory::System, TEXT("Sys"), Log_Filter_Row);
			}
		}
		else
		{
			BuildLayout();
			// Left edge, vertically biased above the chat row. Size/pos/anchors
			// are UPROPERTY-tunable without touching code or Blueprints.
			SetAnchorsInViewport(LogAnchors);
			SetAlignmentInViewport(LogAlignment);
			SetPositionInViewport(LogPosition, false);
		}
	}
	SetVisibility(ESlateVisibility::Visible);

	if (LogManager)
		RebuildVisible();
}

void UCombatLogWidget::NativeDestruct()
{
	if (LogManager)
		LogManager->OnLogEntryAdded.RemoveDynamic(this, &UCombatLogWidget::HandleLogEntryAdded);
	LogManager = nullptr;
	Super::NativeDestruct();
}

void UCombatLogWidget::BuildLayout()
{
	if (LogScroll) return; // Already built (e.g. BP subclass calling up).

	UWidgetTree* Tree = WidgetTree;
	if (!Tree) return;

	USizeBox* Root = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Root->SetWidthOverride(LogSize.X);
	Root->SetHeightOverride(LogSize.Y);
	// Transparent areas must not eat world clicks (the log sits above the
	// HUD): children stay interactive, the box itself never hit-tests.
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Tree->RootWidget = Root;

	UVerticalBox* Col = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Root->AddChild(Col);

	UHorizontalBox* FilterRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Col->AddChildToVerticalBox(FilterRow);

	AddFilterCheck(ECombatLogCategory::Damage, TEXT("Dmg"), FilterRow);
	AddFilterCheck(ECombatLogCategory::Heal, TEXT("Heal"), FilterRow);
	AddFilterCheck(ECombatLogCategory::Loot, TEXT("Loot"), FilterRow);
	AddFilterCheck(ECombatLogCategory::Experience, TEXT("XP"), FilterRow);
	AddFilterCheck(ECombatLogCategory::System, TEXT("Sys"), FilterRow);

	LogScroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	LogScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	LogScroll->OnUserScrolled.AddDynamic(this, &UCombatLogWidget::HandleUserScrolled);
	if (UVerticalBoxSlot* FillSlot = Cast<UVerticalBoxSlot>(Col->AddChildToVerticalBox(LogScroll)))
	{
		FillSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

UCheckBox* UCombatLogWidget::AddFilterCheck(ECombatLogCategory Category, const FString& Label, UPanelWidget* Row)
{
	if (!WidgetTree || !Row) return nullptr;

	const FLinearColor ChannelColor = ColorFor(Category);
	const FText LabelText = FText::FromString(Label);
	const bool bChecked = FCombatLogRule::IsShown(FilterMask, Category);

	if (LogFilterWidgetClass)
	{
		UCombatLogFilterWidget* Cell = CreateWidget<UCombatLogFilterWidget>(this, LogFilterWidgetClass);
		if (!Cell) return nullptr;
		Cell->Setup(Category, LabelText, ChannelColor, bChecked);
		Cell->OnFilterToggled.AddDynamic(this, &UCombatLogWidget::HandleCustomFilterToggled);
		Row->AddChild(Cell);
		CustomFilters.Add(Category, Cell);
		return Cell->GetFilterCheckBox();
	}

	UCheckBox* Box = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	Box->SetIsChecked(bChecked);
	Box->SetCheckedState(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	Box->OnCheckStateChanged.AddDynamic(this, &UCombatLogWidget::HandleFilterChanged);

	UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Txt->SetText(LabelText);
	Txt->SetColorAndOpacity(FSlateColor(ChannelColor));

	UHorizontalBox* Cell = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Cell->AddChildToHorizontalBox(Box);
	if (UHorizontalBoxSlot* TxtSlot = Cast<UHorizontalBoxSlot>(Cell->AddChildToHorizontalBox(Txt)))
	{
		TxtSlot->SetVerticalAlignment(VAlign_Center);
	}
	// HorizontalBox-specific padding is best-effort: any other UPanelWidget
	// subclass still gets the cell (AddChild), just without the gap.
	if (UPanelSlot* CellSlot = Row->AddChild(Cell))
	{
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(CellSlot))
			HSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	// Channel is tracked in the FilterBoxes map (reverse lookup on toggle).
	FilterBoxes.Add(Category, Box);
	return Box;
}

void UCombatLogWidget::BindToManager(UCombatLogManager* InManager)
{
	if (LogManager)
		LogManager->OnLogEntryAdded.RemoveDynamic(this, &UCombatLogWidget::HandleLogEntryAdded);
	LogManager = InManager;
	if (LogManager)
	{
		LogManager->OnLogEntryAdded.AddDynamic(this, &UCombatLogWidget::HandleLogEntryAdded);
		RebuildVisible();
	}
}

void UCombatLogWidget::ClearLog()
{
	if (LogScroll)
		LogScroll->ClearChildren();
	LineWidgets.Reset();
}

void UCombatLogWidget::SetCategoryVisible(ECombatLogCategory Category, bool bVisible)
{
	if (bVisible)
		FilterMask |= FCombatLogRule::MaskFor(Category);
	else
		FilterMask &= ~FCombatLogRule::MaskFor(Category);

	if (UCheckBox** Found = FilterBoxes.Find(Category))
	{
		if (*Found)
			(*Found)->SetIsChecked(bVisible);
	}
	if (UCombatLogFilterWidget** Custom = CustomFilters.Find(Category))
	{
		if (*Custom)
			(*Custom)->SetChannelChecked(bVisible);
	}
	RebuildVisible();
}

bool UCombatLogWidget::IsCategoryVisible(ECombatLogCategory Category) const
{
	return FCombatLogRule::IsShown(FilterMask, Category);
}

void UCombatLogWidget::HandleLogEntryAdded(const FCombatLogEntry& Entry)
{
	AppendEntry(Entry);
}

void UCombatLogWidget::AppendEntry(const FCombatLogEntry& Entry)
{
	if (!LogScroll || !FCombatLogRule::IsShown(FilterMask, Entry.Category))
		return;

	const FLinearColor LineColor = ColorFor(Entry.Category);
	FText TimeText = FText::GetEmpty();
	if (bShowTimestamps)
		TimeText = FText::FromString(FString::Printf(TEXT("[%s]"),
			*Entry.Timestamp.ToString(TEXT("%H:%M:%S"))));

	if (LogRowWidgetClass)
	{
		UCombatLogRowWidget* Row = CreateWidget<UCombatLogRowWidget>(this, LogRowWidgetClass);
		if (!Row) return;
		Row->SetRowData(Entry.Category, TimeText, FText::FromString(Entry.Text), LineColor);
		LogScroll->AddChild(Row);
		LineWidgets.Add(Row);
	}
	else
	{
		UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FString DisplayText = Entry.Text;
		if (bShowTimestamps)
			DisplayText = FString::Printf(TEXT("[%s] %s"), *Entry.Timestamp.ToString(TEXT("%H:%M:%S")), *Entry.Text);
		Line->SetText(FText::FromString(DisplayText));
		Line->SetColorAndOpacity(FSlateColor(LineColor));
		FSlateFontInfo LineFont = Line->GetFont();
		LineFont.Size = LineFontSize;
		Line->SetFont(LineFont);
		Line->SetShadowColorAndOpacity(FLinearColor::Black);
		Line->SetShadowOffset(FVector2D(1.f, 1.f));
		Line->SetAutoWrapText(true);
		LogScroll->AddChild(Line);
		LineWidgets.Add(Line);
	}

	TrimLines();
	if (bFollowTail)
		LogScroll->ScrollToEnd();
}

void UCombatLogWidget::RebuildVisible()
{
	ClearLog();
	if (!LogManager) return;
	bFollowTail = true;
	for (const FCombatLogEntry& Entry : LogManager->GetEntries())
		AppendEntry(Entry);
	if (LogScroll)
		LogScroll->ScrollToEnd();
}

void UCombatLogWidget::TrimLines()
{
	while (LineWidgets.Num() > MaxLines && LineWidgets.Num() > 0)
	{
		if (UWidget* Old = LineWidgets[0])
			Old->RemoveFromParent();
		LineWidgets.RemoveAt(0);
	}
}

bool UCombatLogWidget::IsAtBottom() const
{
	if (!LogScroll) return true;
	return LogScroll->GetScrollOffset() >= LogScroll->GetScrollOffsetOfEnd() - 20.f;
}

FLinearColor UCombatLogWidget::ColorFor(ECombatLogCategory Category) const
{
	if (!bOverrideCategoryColors)
		return FCombatLogRule::ColorFor(Category);
	switch (Category)
	{
	case ECombatLogCategory::Damage:     return DamageColor;
	case ECombatLogCategory::Heal:       return HealColor;
	case ECombatLogCategory::Loot:       return LootColor;
	case ECombatLogCategory::Experience: return ExperienceColor;
	case ECombatLogCategory::System:
	default:                            return SystemColor;
	}
}

void UCombatLogWidget::HandleUserScrolled(float /*CurrentOffset*/)
{
	bFollowTail = IsAtBottom();
}

void UCombatLogWidget::HandleFilterChanged(bool /*bIsChecked*/)
{
	// Recompute the mask from the live checkbox states (single source of
	// truth is the boxes themselves, so BP-driven SetIsChecked just works).
	uint8 NewMask = 0;
	for (const auto& Pair : FilterBoxes)
	{
		if (Pair.Value && Pair.Value->IsChecked())
			NewMask |= FCombatLogRule::MaskFor(Pair.Key);
	}
	for (const auto& Pair : CustomFilters)
	{
		if (Pair.Value && Pair.Value->IsChannelChecked())
			NewMask |= FCombatLogRule::MaskFor(Pair.Key);
	}
	FilterMask = NewMask;
	RebuildVisible();
}

void UCombatLogWidget::HandleCustomFilterToggled()
{
	HandleFilterChanged(true);
}
