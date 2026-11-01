#include "UI/PlayerInspectWidget.h"
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

void UPlayerInspectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Blueprint layout wins when bound; otherwise build the code fallback.
	// Widget-level placement stays in code (viewport anchors are runtime-only).
	if (!bWired)
	{
		bWired = true;
		if (Inspect_Title_Text || Inspect_Gear_Box || Inspect_Close_Btn)
		{
			HeaderText = Inspect_Title_Text;
			GearBox = Inspect_Gear_Box;
			if (Inspect_Close_Btn)
				Inspect_Close_Btn->OnClicked.AddDynamic(this, &UPlayerInspectWidget::HideInspect);
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
	{
		// Display preview for icons/tooltips (read-only snapshot — the
		// server never sends inventory rows here).
		FInventoryItemStruct Preview;
		Preview.itemId = Gear.ItemId;
		Preview.slug = Gear.ItemSlug;
		Preview.name = LocalizeItem(Gear.ItemSlug);
		Preview.quantity = 1;
		AddGearSlotRow(Gear.SlotSlug, Preview);
	}
	if (Data.Attributes.Num() > 0)
	{
		AddGearRow(TEXT("Stats"), TEXT(""));
		for (const FPlayerInspectAttrEntry& Attr : Data.Attributes)
		{
			FString Pretty = Attr.Slug.Replace(TEXT("_"), TEXT(" "));
			AddGearRow(Pretty, FString::FromInt(Attr.Value));
		}
	}
	if (Data.Gear.Num() == 0 && Data.RaceSlug.IsEmpty() && Data.Attributes.Num() == 0 && GearBox)
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
	GearPreviews.Reset();
	if (InspectTooltip)
		InspectTooltip->HideTooltip();
}

void UPlayerInspectWidget::AddGearSlotRow(const FString& SlotName, const FInventoryItemStruct& PreviewItem)
{
	if (!GearBox || !WidgetTree) return;

	// Icon slots need the vendor slot BP; without it fall back to text.
	if (!InspectSlotClass)
	{
		AddGearRow(SlotName, PreviewItem.name);
		return;
	}

	const int32 SlotIndex = GearPreviews.Num();
	GearPreviews.Add(PreviewItem);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetText(FText::FromString(SlotName + TEXT(":")));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetShadowColorAndOpacity(FLinearColor::Black);
	Label->SetShadowOffset(FVector2D(1.f, 1.f));
	Label->SetAutoWrapText(true);
	if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(Row->AddChildToHorizontalBox(Label)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	if (UVendorSlotWidget* GearSlot = CreateWidget<UVendorSlotWidget>(this, InspectSlotClass))
	{
		// Display only: never in cart, never disabled by trade rules here —
		// equipped gear is shown as-is. Clicks are ignored (no subscription).
		GearSlot->SetupAsInvSlot(SlotIndex, PreviewItem, false, false, 0);
		GearSlot->OnVendorSlotHovered.AddDynamic(this, &UPlayerInspectWidget::HandleGearSlotHovered);
		Row->AddChildToHorizontalBox(GearSlot);
	}

	GearBox->AddChild(Row);
}

void UPlayerInspectWidget::EnsureTooltip()
{
	if (InspectTooltip || !InspectTooltipWidgetClass || !GetWorld()) return;
	InspectTooltip = CreateWidget<UItemTooltipWidget>(GetWorld(), InspectTooltipWidgetClass);
	if (InspectTooltip)
	{
		InspectTooltip->AddToViewport(1200);
		InspectTooltip->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPlayerInspectWidget::HandleGearSlotHovered(int32 SlotIndex, bool bIsHovered)
{
	if (!bIsHovered || !GearPreviews.IsValidIndex(SlotIndex))
	{
		if (InspectTooltip)
			InspectTooltip->HideTooltip();
		return;
	}
	if (!InspectTooltipWidgetClass) return;
	EnsureTooltip();
	if (!InspectTooltip) return;
	InspectTooltip->SetItemData(GearPreviews[SlotIndex]);
	FVector2D Mouse = FVector2D::ZeroVector;
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		PC->GetMousePosition(Mouse.X, Mouse.Y);
	InspectTooltip->UpdateTooltipPosition(Mouse);
	InspectTooltip->ShowTooltip();
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
