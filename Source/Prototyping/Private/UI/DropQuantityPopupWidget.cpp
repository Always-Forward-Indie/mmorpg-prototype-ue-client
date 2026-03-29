#include "UI/DropQuantityPopupWidget.h"

void UDropQuantityPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UDropQuantityPopupWidget::OnConfirmClicked);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UDropQuantityPopupWidget::OnCancelClicked);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UDropQuantityPopupWidget::ShowForItem(const FInventoryItemStruct& InItem)
{
	PendingItem = InItem;
	MaxQuantity = FMath::Max(1, InItem.quantity);

	if (ItemNameText)
	{
		ItemNameText->SetText(FText::FromString(InItem.slug));
	}

	if (QuantityInput)
	{
		QuantityInput->SetText(FText::AsNumber(MaxQuantity));
	}

	SetVisibility(ESlateVisibility::Visible);

	if (QuantityInput)
	{
		QuantityInput->SetKeyboardFocus();
	}
}

void UDropQuantityPopupWidget::HidePopup()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDropQuantityPopupWidget::OnConfirmClicked()
{
	int32 Qty = 1;
	if (QuantityInput)
	{
		const FString RawText = QuantityInput->GetText().ToString();
		const int32 Parsed = FCString::Atoi(*RawText);
		Qty = FMath::Clamp(Parsed, 1, MaxQuantity);
	}

	HidePopup();
	OnDropQuantityConfirmed.Broadcast(PendingItem, Qty);
}

void UDropQuantityPopupWidget::OnCancelClicked()
{
	HidePopup();
	OnDropQuantityCancelled.Broadcast();
}
