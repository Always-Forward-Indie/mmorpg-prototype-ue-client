#include "UI/DropQuantityPopupWidget.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

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

	if (MaxButton)
	{
		MaxButton->OnClicked.AddDynamic(this, &UDropQuantityPopupWidget::HandleMax);
	}

	if (DecreaseButton)
	{
		DecreaseButton->OnClicked.AddDynamic(this, &UDropQuantityPopupWidget::HandleDecrease);
	}

	if (IncreaseButton)
	{
		IncreaseButton->OnClicked.AddDynamic(this, &UDropQuantityPopupWidget::HandleIncrease);
	}

	if (QuantityInput)
	{
		QuantityInput->OnTextCommitted.AddDynamic(this, &UDropQuantityPopupWidget::HandleQuantityTextCommitted);
		QuantityInput->OnTextChanged.AddDynamic(this, &UDropQuantityPopupWidget::HandleQuantityTextChanged);
	}

	SetVisibility(ESlateVisibility::Collapsed);
}

void UDropQuantityPopupWidget::ShowForItem(const FInventoryItemStruct& InItem)
{
	PendingItem = InItem;
	MaxQuantity = FMath::Max(1, InItem.quantity);
	CurrentQty = MaxQuantity;

	if (ItemNameText)
	{
		FText ItemName = FText::FromString(InItem.slug);
		if (!InItem.slug.IsEmpty())
		{
			if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
			{
				if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
				{
					const FText LocName = Loc->GetItemDisplayName(InItem.slug);
					if (!LocName.IsEmpty())
					{
						ItemName = LocName;
					}
				}
			}
		}
		ItemNameText->SetText(ItemName);
	}

	if (QuantityInput)
	{
		QuantityInput->SetText(FText::AsNumber(CurrentQty));
	}

	SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
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
	HidePopup();
	OnDropQuantityConfirmed.Broadcast(PendingItem, CurrentQty);
}

void UDropQuantityPopupWidget::OnCancelClicked()
{
	HidePopup();
	OnDropQuantityCancelled.Broadcast();
}

void UDropQuantityPopupWidget::HandleMax()
{
	SetQuantity(MaxQuantity);
}

void UDropQuantityPopupWidget::HandleDecrease()
{
	SetQuantity(CurrentQty - 1);
}

void UDropQuantityPopupWidget::HandleIncrease()
{
	SetQuantity(CurrentQty + 1);
}

void UDropQuantityPopupWidget::SetQuantity(int32 NewQty)
{
	CurrentQty = FMath::Clamp(NewQty, 1, MaxQuantity);

	if (QuantityInput)
	{
		QuantityInput->SetText(FText::AsNumber(CurrentQty));
	}

	if (DecreaseButton)
	{
		DecreaseButton->SetIsEnabled(CurrentQty > 1);
	}
	if (IncreaseButton)
	{
		IncreaseButton->SetIsEnabled(CurrentQty < MaxQuantity);
	}
}

void UDropQuantityPopupWidget::HandleQuantityTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		const int32 Parsed = FCString::Atoi(*Text.ToString());
		SetQuantity(Parsed);
	}
}

void UDropQuantityPopupWidget::HandleQuantityTextChanged(const FText& Text)
{
	FString Str = Text.ToString();
	FString Filtered;
	for (const TCHAR Ch : Str)
	{
		if (FChar::IsDigit(Ch))
			Filtered.AppendChar(Ch);
	}
	if (Filtered.Len() > 3)
	{
		Filtered.LeftChopInline(Filtered.Len() - 3);
	}
	if (Filtered != Str)
	{
		QuantityInput->SetText(FText::FromString(Filtered));
	}
}

FReply UDropQuantityPopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (PopupContent && PopupContent->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Unhandled();
	}
	OnCancelClicked();
	return FReply::Handled();
}
