#include "UI/EmoteItemWidget.h"
#include "Data/EmoteDefinitionTable.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

void UEmoteItemWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &UEmoteItemWidget::HandleButtonClicked);
    }

    if (SlotBorder)
    {
        SlotBorder->SetBrushColor(NormalTint);
    }
}

void UEmoteItemWidget::NativeDestruct()
{
    if (SlotButton)
    {
        SlotButton->OnClicked.RemoveDynamic(this, &UEmoteItemWidget::HandleButtonClicked);
    }
    Super::NativeDestruct();
}

void UEmoteItemWidget::SetEmoteData(const FEmoteDefinitionData& InEmoteDef, UDataTable* InEmoteTable)
{
    EmoteDef = InEmoteDef;

    // Look up the visual row from the DataTable
    const FEmoteTableRow* Row = nullptr;
    if (InEmoteTable && !InEmoteDef.slug.IsEmpty())
    {
        Row = InEmoteTable->FindRow<FEmoteTableRow>(FName(*InEmoteDef.slug), TEXT("EmoteItemWidget"), false);
    }

    // Icon
    if (EmoteIcon_Image)
    {
        UTexture2D* Icon = (Row && !Row->Icon.IsNull()) ? Row->Icon.LoadSynchronous() : nullptr;
        if (Icon)
        {
            EmoteIcon_Image->SetBrushFromTexture(Icon, false);
        }
        else
        {
            EmoteIcon_Image->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    // Name — prefer localized name from table, fall back to server displayName
    if (EmoteName_Text)
    {
        if (Row && !Row->LocalizedName.IsEmpty())
        {
            EmoteName_Text->SetText(Row->LocalizedName);
        }
        else
        {
            EmoteName_Text->SetText(FText::FromString(InEmoteDef.displayName));
        }
    }
}

void UEmoteItemWidget::HandleButtonClicked()
{
    OnEmoteItemClicked.Broadcast(EmoteDef);
}

void UEmoteItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    if (SlotBorder)
    {
        SlotBorder->SetBrushColor(HoverTint);
    }
    OnEmoteItemHovered.Broadcast(EmoteDef, true);
}

void UEmoteItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    if (SlotBorder)
    {
        SlotBorder->SetBrushColor(NormalTint);
    }
    OnEmoteItemHovered.Broadcast(EmoteDef, false);
}
