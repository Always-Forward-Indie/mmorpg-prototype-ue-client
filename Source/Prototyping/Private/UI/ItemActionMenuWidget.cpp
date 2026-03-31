#include "UI/ItemActionMenuWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetLayoutLibrary.h"

// ---------------------------------------------------------------------------
// UItemActionMenuWidget
// ---------------------------------------------------------------------------

UItemActionMenuWidget::UItemActionMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentItem = FInventoryItemStruct();
}

void UItemActionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// Input - the entire menu is a single mouse-capture target.
// On LMB Down: capture + remember which row was pressed.
// On LMB Up:   fire action only if the cursor is still on the same row.
// RMB / any other button: consume so nothing leaks through.
// ---------------------------------------------------------------------------

FReply UItemActionMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		PressedRowIndex = HitTestRowIndex(InMouseEvent.GetScreenSpacePosition());
		if (TSharedPtr<SWidget> Slate = GetCachedWidget())
		{
			return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
		}
		return FReply::Handled();
	}
	// Consume RMB and all other buttons - do not let them bubble
	return FReply::Handled();
}

FReply UItemActionMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 UpRowIndex = HitTestRowIndex(InMouseEvent.GetScreenSpacePosition());
		const int32 DownIndex  = PressedRowIndex;
		PressedRowIndex = -1;

		// Fire only if both press and release land on the same valid row
		if (DownIndex >= 0 && DownIndex == UpRowIndex && DownIndex < RowActions.Num())
		{
			ExecuteAction(RowActions[DownIndex]);
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void UItemActionMenuWidget::SetManagers(UInventoryManager* InInventoryManager, UEquipmentManager* InEquipmentManager)
{
	InventoryManager = InInventoryManager;
	EquipmentManager = InEquipmentManager;
}

void UItemActionMenuWidget::ShowForItem(const FInventoryItemStruct& InItem, FVector2D ScreenPosition)
{
	CurrentItem = InItem;
	RebuildActions();

	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	SetPositionInViewport(ScreenPosition / Scale, false);
	SetVisibility(ESlateVisibility::Visible);
}

void UItemActionMenuWidget::HideMenu()
{
	PressedRowIndex = -1;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UItemActionMenuWidget::RebuildActions()
{
	if (!ActionList)
	{
		UE_LOG(LogTemp, Error, TEXT("ItemActionMenuWidget: ActionList VerticalBox is not bound. "
			"Please add a VerticalBox named 'ActionList' to your Blueprint widget."));
		return;
	}

	RowActions.Empty();
	ActionList->ClearChildren();

	if (CurrentItem.itemId <= 0) return;

	if (CurrentItem.isEquippable && !CurrentItem.is_equipped)
		AddActionRow(FText::FromString(TEXT("Equip")), EItemContextAction::Equip);

	if (CurrentItem.isEquippable && CurrentItem.is_equipped)
		AddActionRow(FText::FromString(TEXT("Unequip")), EItemContextAction::Unequip);

	if (CurrentItem.isUsable)
		AddActionRow(FText::FromString(TEXT("Use")), EItemContextAction::Use);

	if (!CurrentItem.isQuestItem)
		AddActionRow(FText::FromString(TEXT("Drop")), EItemContextAction::Drop);
}

void UItemActionMenuWidget::AddActionRow(const FText& Label, EItemContextAction Action)
{
	if (!ActionList) return;

	// Visual: Border > TextBlock
	UBorder* Bg = NewObject<UBorder>(this);
	Bg->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.9f));
	Bg->SetPadding(FMargin(12.f, 6.f));
	Bg->SetHorizontalAlignment(HAlign_Fill);

	UTextBlock* Txt = NewObject<UTextBlock>(this);
	Txt->SetText(Label);
	Txt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Bg->AddChild(Txt);

	RowActions.Add(Action);

	if (UVerticalBoxSlot* BoxSlot = ActionList->AddChildToVerticalBox(Bg))
	{
		BoxSlot->SetPadding(FMargin(0.f, 1.f));
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

int32 UItemActionMenuWidget::HitTestRowIndex(const FVector2D& AbsoluteScreenPos) const
{
	if (!ActionList) return -1;

	for (int32 i = 0; i < ActionList->GetChildrenCount(); ++i)
	{
		UWidget* Child = ActionList->GetChildAt(i);
		if (!Child) continue;

		const FGeometry Geo = Child->GetCachedGeometry();
		const FVector2D Local = Geo.AbsoluteToLocal(AbsoluteScreenPos);
		const FVector2D Size  = Geo.GetLocalSize();

		if (Local.X >= 0.f && Local.X <= Size.X && Local.Y >= 0.f && Local.Y <= Size.Y)
		{
			return i;
		}
	}
	return -1;
}

void UItemActionMenuWidget::ExecuteAction(EItemContextAction Action)
{
	OnActionSelected.Broadcast(Action, CurrentItem);

	if (InventoryManager)
	{
		const int32 CharacterId = InventoryManager->GetInventory().characterId;

		switch (Action)
		{
		case EItemContextAction::Use:
			InventoryManager->UseItem(CurrentItem.itemId);
			break;

		case EItemContextAction::Drop:
			// Drop is handled exclusively by InventoryWidget via OnActionSelected
			break;

		case EItemContextAction::Equip:
			if (EquipmentManager)
				EquipmentManager->RequestEquipItem(CharacterId, CurrentItem.id);
			break;

		case EItemContextAction::Unequip:
			if (EquipmentManager && !CurrentItem.equipSlotSlug.IsEmpty())
				EquipmentManager->RequestUnequipItem(CharacterId, CurrentItem.equipSlotSlug);
			break;
		}
	}

	HideMenu();
}
