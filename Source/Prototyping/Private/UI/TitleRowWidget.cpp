#include "UI/TitleRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UTitleRowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Row_Equip_Button)
        Row_Equip_Button->OnClicked.AddDynamic(this, &UTitleRowWidget::HandleEquipClicked);
}

void UTitleRowWidget::Populate(const FTitleEntry& Entry, bool bIsEquipped)
{
    RowSlug        = Entry.slug;
    bRowIsEquipped = bIsEquipped;

    if (Row_Name_Text)
        Row_Name_Text->SetText(FText::FromString(
            bIsEquipped
                ? FString::Printf(TEXT("[E] %s"), *Entry.displayName)
                : Entry.displayName));

    if (Row_Bonus_Text)
        Row_Bonus_Text->SetText(FText::FromString(BuildBonusSummary(Entry.bonuses)));

    if (Row_Equip_Button)
        Row_Equip_Button->SetIsEnabled(!bIsEquipped);
}

void UTitleRowWidget::HandleEquipClicked()
{
    // Equipped title has button disabled, but guard anyway.
    if (!bRowIsEquipped)
        OnEquipRequested.Broadcast(RowSlug);
}

FString UTitleRowWidget::BuildBonusSummary(const TArray<FTitleBonusEntry>& Bonuses)
{
    TArray<FString> Parts;
    Parts.Reserve(Bonuses.Num());
    for (const FTitleBonusEntry& B : Bonuses)
    {
        const FString Sign = B.value >= 0.0f ? TEXT("+") : TEXT("");
        Parts.Add(FString::Printf(TEXT("%s%.0f %s"), *Sign, B.value, *B.attributeSlug));
    }
    return FString::Join(Parts, TEXT(", "));
}
