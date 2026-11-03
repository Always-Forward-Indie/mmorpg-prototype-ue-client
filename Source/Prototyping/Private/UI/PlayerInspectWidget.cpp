#include "UI/PlayerInspectWidget.h"
#include "Prototyping.h"
#include "Blueprint/WidgetLayoutLibrary.h"
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

	// Blueprint layout wins when bound — including placement: the BP root
	// carries its own anchors, so code must not override them. The code
	// fallback positions itself because it has no designer layout.
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
			SetAnchorsInViewport(FAnchors(0.f, 0.5f));
			SetAlignmentInViewport(FVector2D(0.f, 0.5f));
			SetPositionInViewport(WindowPosition, false);
		}
	}
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
	// Transparent areas must not eat world clicks (window sits above HUD).
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
	// Instance wrapper must never eat hits (window sits above HUD).
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UPlayerInspectWidget::HideInspect()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UPlayerInspectWidget::IsInspectOpen() const
{
	return IsVisible();
}

FReply UPlayerInspectWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !DragHandle)
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	// Drag only from the handle; everywhere else behaves as before.
	const FVector2D Local = DragHandle->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	const FVector2D Size = DragHandle->GetCachedGeometry().GetLocalSize();
	const bool bOnHandle = Local.X >= 0.f && Local.Y >= 0.f && Local.X <= Size.X && Local.Y <= Size.Y;
	if (!bOnHandle)
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		// Seed from the live geometry so BP-anchored placement is preserved.
		const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
		const FVector2D AbsPos = GetCachedGeometry().GetAbsolutePosition();
		DragOffset = InMouseEvent.GetScreenSpacePosition() / Scale
			- FVector2D(AbsPos.X / Scale, AbsPos.Y / Scale);
		bDragging = true;
		if (TSharedPtr<SWidget> Slate = GetCachedWidget())
			return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPlayerInspectWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		if (TSharedPtr<SWidget> Slate = GetCachedWidget())
			return FReply::Handled().ReleaseMouseCapture();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UPlayerInspectWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging) { UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition()); return FReply::Handled(); }
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UPlayerInspectWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
	if (!GetWorld() || !GetWorld()->GetFirstPlayerController()) return;
	int32 W = 0, H = 0;
	GetWorld()->GetFirstPlayerController()->GetViewportSize(W, H);
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	const FVector2D VP = FVector2D(W, H) / Scale;
	FVector2D Size = GetDesiredSize();
	if (Size.IsZero()) Size = FVector2D(340.f, 420.f);
	const FVector2D Raw = ScreenCursorPos / Scale - DragOffset;
	FVector2D Pos(FMath::Clamp(Raw.X, 0.f, FMath::Max(0.f, VP.X - Size.X)),
		FMath::Clamp(Raw.Y, 0.f, FMath::Max(0.f, VP.Y - Size.Y)));
	SetPositionInViewport(Pos, false);
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
