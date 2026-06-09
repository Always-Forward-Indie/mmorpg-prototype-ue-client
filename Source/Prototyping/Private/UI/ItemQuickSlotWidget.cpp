#include "UI/ItemQuickSlotWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Gameplay/Items/InventoryManager.h"

void UItemQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ItemButton)
		ItemButton->OnClicked.AddDynamic(this, &UItemQuickSlotWidget::OnSlotClicked);

	if (CooldownOverlay)
		CooldownOverlay->SetVisibility(ESlateVisibility::Collapsed);

	if (QuantityText)
		QuantityText->SetVisibility(ESlateVisibility::Collapsed);

	if (ItemIcon)
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
}

void UItemQuickSlotWidget::Setup(UInventoryManager* InInventoryManager, int32 InSlotIndex, const FKey& InBoundKey)
{
	InventoryManager = InInventoryManager;
	SlotData.slotIndex = InSlotIndex;
	SlotData.boundKey = InBoundKey;

	if (HotkeyText)
		HotkeyText->SetText(FText::FromString(InBoundKey.ToString()));
}

void UItemQuickSlotWidget::AssignItem(int32 ItemId, const FString& ItemSlug, int32 Quantity, const FString& InIconPath)
{
	SlotData.itemId = ItemId;
	SlotData.itemSlug = ItemSlug;
	SlotData.quantity = Quantity;
	SlotData.iconPath = InIconPath;

	if (!InIconPath.IsEmpty())
	{
		UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *InIconPath);
		if (ItemIcon && Tex)
		{
			ItemIcon->SetBrushFromTexture(Tex);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Quantity));
		QuantityText->SetVisibility(Quantity > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (EmptySlotIcon)
		EmptySlotIcon->SetVisibility(ESlateVisibility::Collapsed);
}

void UItemQuickSlotWidget::ClearSlot()
{
	SlotData.itemId = 0;
	SlotData.itemSlug.Empty();
	SlotData.quantity = 0;
	SlotData.iconPath.Empty();

	if (ItemIcon)
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);

	if (QuantityText)
		QuantityText->SetVisibility(ESlateVisibility::Collapsed);

	if (EmptySlotIcon)
		EmptySlotIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UItemQuickSlotWidget::UpdateQuantity(int32 NewQuantity)
{
	SlotData.quantity = NewQuantity;

	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(NewQuantity));
		QuantityText->SetVisibility(NewQuantity > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (NewQuantity <= 0)
		ClearSlot();
}

void UItemQuickSlotWidget::UseItem()
{
	if (SlotData.itemId <= 0 || !InventoryManager) return;

	InventoryManager->UseItem(SlotData.itemId, 1);
	OnQuickSlotUsed.Broadcast(SlotData.slotIndex);
}

void UItemQuickSlotWidget::OnSlotClicked()
{
	UseItem();
}

FReply UItemQuickSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && !IsEmpty())
	{
		ClearSlot();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
