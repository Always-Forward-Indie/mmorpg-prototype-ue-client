#include "UI/PlayerInspectWidget.h"
#include "Prototyping.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UPlayerInspectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildLayout();

	SetAnchorsInViewport(FAnchors(0.f, 0.5f));
	SetAlignmentInViewport(FVector2D(0.f, 0.5f));
	SetPositionInViewport(WindowPosition, false);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerInspectWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UPlayerInspectWidget::BuildLayout()
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
	HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f)));
	HeaderText->SetShadowColorAndOpacity(FLinearColor::Black);
	HeaderText->SetShadowOffset(FVector2D(1.f, 1.f));
	if (UVerticalBoxSlot* HeadSlot = Cast<UVerticalBoxSlot>(RootCol->AddChildToVerticalBox(HeaderText)))
		HeadSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	GearBox = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	if (UVerticalBoxSlot* FillSlot = Cast<UVerticalBoxSlot>(RootCol->AddChildToVerticalBox(GearBox)))
		FillSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* CloseBtn = Tree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* CloseTxt = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloseTxt->SetText(FText::FromString(TEXT("Close")));
	CloseBtn->AddChild(CloseTxt);
	CloseBtn->OnClicked.AddDynamic(this, &UPlayerInspectWidget::HideInspect);
	RootCol->AddChildToVerticalBox(CloseBtn);
}

void UPlayerInspectWidget::ShowInspect(const FPlayerInspectData& Data)
{
	ClearGear();
	const FString Head = Data.CharacterLevel > 0
		? FString::Printf(TEXT("%s  [%d]  %s"), *Data.CharacterName, Data.CharacterLevel, *Data.ClassSlug)
		: Data.CharacterName;
	if (HeaderText)
		HeaderText->SetText(FText::FromString(Head));

	if (!Data.RaceSlug.IsEmpty())
		AddGearRow(TEXT("Race"), Data.RaceSlug);
	for (const FPlayerInspectGearEntry& Gear : Data.Gear)
		AddGearRow(Gear.SlotSlug, LocalizeItem(Gear.ItemSlug));
	if (Data.Gear.Num() == 0 && Data.RaceSlug.IsEmpty() && GearBox)
	{
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Empty->SetText(FText::FromString(TEXT("No equipment")));
		Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
		GearBox->AddChild(Empty);
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UPlayerInspectWidget::HideInspect()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UPlayerInspectWidget::IsInspectOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UPlayerInspectWidget::ClearGear()
{
	if (GearBox)
		GearBox->ClearChildren();
}

void UPlayerInspectWidget::AddGearRow(const FString& SlotName, const FString& ItemName)
{
	if (!GearBox || !WidgetTree) return;
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Line->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"), *SlotName, *ItemName)));
	Line->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Line->SetShadowColorAndOpacity(FLinearColor::Black);
	Line->SetShadowOffset(FVector2D(1.f, 1.f));
	Line->SetAutoWrapText(true);
	GearBox->AddChild(Line);
}

FString UPlayerInspectWidget::LocalizeItem(const FString& Slug) const
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
