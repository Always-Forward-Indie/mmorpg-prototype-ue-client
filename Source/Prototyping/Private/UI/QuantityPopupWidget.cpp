#include "UI/QuantityPopupWidget.h"

void UQuantityPopupWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (DecreaseButton) DecreaseButton->OnClicked.AddDynamic(this, &UQuantityPopupWidget::HandleDecrease);
    if (IncreaseButton) IncreaseButton->OnClicked.AddDynamic(this, &UQuantityPopupWidget::HandleIncrease);
    if (MaxButton)      MaxButton->OnClicked.AddDynamic(this, &UQuantityPopupWidget::HandleMax);
    if (ConfirmButton)  ConfirmButton->OnClicked.AddDynamic(this, &UQuantityPopupWidget::HandleConfirm);
    if (RemoveButton)   RemoveButton->OnClicked.AddDynamic(this, &UQuantityPopupWidget::HandleRemove);
    if (CancelButton)   CancelButton->OnClicked.AddDynamic(this, &UQuantityPopupWidget::HandleCancel);
    if (QuantityInput)  QuantityInput->OnTextCommitted.AddDynamic(this, &UQuantityPopupWidget::HandleQuantityTextCommitted);
    if (QuantityInput)  QuantityInput->OnTextChanged.AddDynamic(this, &UQuantityPopupWidget::HandleQuantityTextChanged);

    SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// Open
// ---------------------------------------------------------------------------

void UQuantityPopupWidget::OpenForAdd(int32 InSlotIndex, const FString& InName,
    int32 InPricePerUnit, int32 InMaxQuantity, int32 InInitialQuantity)
{
    SlotIndex    = InSlotIndex;
    PricePerUnit = InPricePerUnit;
    MaxQuantity  = FMath::Max(1, InMaxQuantity);
    bUpdateMode  = false;

    if (ItemNameText)
        ItemNameText->SetText(FText::FromString(InName));

    if (PricePerUnitText)
        PricePerUnitText->SetText(FText::FromString(
            FString::Printf(TEXT("%d g / pc"), InPricePerUnit)));

    if (RemoveButton)
        RemoveButton->SetVisibility(ESlateVisibility::Collapsed);

    SetQuantity(FMath::Clamp(InInitialQuantity, 1, MaxQuantity));
    SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    SetVisibility(ESlateVisibility::Visible);
}

void UQuantityPopupWidget::OpenForUpdate(int32 InSlotIndex, const FString& InName,
    int32 InPricePerUnit, int32 InMaxQuantity, int32 InCurrentQuantity)
{
    SlotIndex    = InSlotIndex;
    PricePerUnit = InPricePerUnit;
    MaxQuantity  = FMath::Max(1, InMaxQuantity);
    bUpdateMode  = true;

    if (ItemNameText)
        ItemNameText->SetText(FText::FromString(InName));

    if (PricePerUnitText)
        PricePerUnitText->SetText(FText::FromString(
            FString::Printf(TEXT("%d g / pc"), InPricePerUnit)));

    if (RemoveButton)
        RemoveButton->SetVisibility(ESlateVisibility::Visible);

    SetQuantity(FMath::Clamp(InCurrentQuantity, 1, MaxQuantity));
    SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    SetVisibility(ESlateVisibility::Visible);
}

void UQuantityPopupWidget::ClosePopup()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void UQuantityPopupWidget::SetQuantity(int32 NewQty)
{
    // In update mode allow 0 (means "remove from cart" on confirm)
    const int32 MinQty = bUpdateMode ? 0 : 1;
    CurrentQty = FMath::Clamp(NewQty, MinQty, MaxQuantity);

    if (QuantityInput)
        QuantityInput->SetText(FText::AsNumber(CurrentQty));

    if (DecreaseButton) DecreaseButton->SetIsEnabled(CurrentQty > MinQty);
    if (IncreaseButton) IncreaseButton->SetIsEnabled(CurrentQty < MaxQuantity);

    UpdateTotalPrice();
}

void UQuantityPopupWidget::UpdateTotalPrice()
{
    if (TotalPriceText)
        TotalPriceText->SetText(FText::FromString(
            FString::Printf(TEXT("Total: %d g"), PricePerUnit * CurrentQty)));
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------

void UQuantityPopupWidget::HandleDecrease()
{
    SetQuantity(CurrentQty - 1);
}

void UQuantityPopupWidget::HandleIncrease()
{
    SetQuantity(CurrentQty + 1);
}

void UQuantityPopupWidget::HandleMax()
{
    SetQuantity(MaxQuantity);
}

void UQuantityPopupWidget::HandleConfirm()
{
    ClosePopup();
    // Qty == 0 in update mode means the player wants to remove the item from the cart
    if (bUpdateMode && CurrentQty == 0)
        OnQuantityRemove.Broadcast(SlotIndex);
    else
        OnQuantityConfirmed.Broadcast(SlotIndex, CurrentQty);
}

void UQuantityPopupWidget::HandleRemove()
{
    ClosePopup();
    OnQuantityRemove.Broadcast(SlotIndex);
}

void UQuantityPopupWidget::HandleCancel()
{
    ClosePopup();
    OnQuantityCancelled.Broadcast();
}

void UQuantityPopupWidget::HandleQuantityTextChanged(const FText& Text)
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

void UQuantityPopupWidget::HandleQuantityTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
    {
        const int32 Parsed = FCString::Atoi(*Text.ToString());
        SetQuantity(Parsed);
    }
}

FReply UQuantityPopupWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (PopupContent && PopupContent->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
    {
        return FReply::Unhandled();
    }
    HandleCancel();
    return FReply::Handled();
}
