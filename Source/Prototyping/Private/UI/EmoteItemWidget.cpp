#include "UI/EmoteItemWidget.h"
#include "Services/LocalizationSubsystem.h"
#include "Utils/JSONParser.h"
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

    // Name — S3c: codebook table row wins when filled; existing curated
    // row/server names cover unfilled tables; humanised slug last.
    if (EmoteName_Text)
    {
        FText Name;
        if (ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this))
        {
            const FText LocName = Loc->GetEmoteDisplayName(InEmoteDef.slug);
            const FString Human = JSONParser::HumanizeSlugForDisplay(InEmoteDef.slug);
            if (!LocName.IsEmpty() && LocName.ToString() != InEmoteDef.slug && LocName.ToString() != Human)
                Name = LocName;
        }
        if (Name.IsEmpty() && Row && !Row->LocalizedName.IsEmpty())
            Name = Row->LocalizedName;
        if (Name.IsEmpty() && !InEmoteDef.displayName.IsEmpty())
            Name = FText::FromString(InEmoteDef.displayName);
        if (Name.IsEmpty())
            Name = FText::FromString(JSONParser::HumanizeSlugForDisplay(InEmoteDef.slug));
        EmoteName_Text->SetText(Name);
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
