#include "UI/TitleRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UTitleRowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Row_Equip_Button)
        Row_Equip_Button->OnClicked.AddDynamic(this, &UTitleRowWidget::HandleEquipClicked);
}

void UTitleRowWidget::Populate(const FTitleEntry& Entry, bool bIsEquipped, const FText& ResolvedName)
{
    RowSlug        = Entry.slug;
    bRowIsEquipped = bIsEquipped;

    if (Row_Name_Text)
    {
        if (bIsEquipped)
        {
            Row_Name_Text->SetText(FText::Format(
                FText::FromString(TEXT("[E] {0}")), ResolvedName));
        }
        else
        {
            Row_Name_Text->SetText(ResolvedName);
        }
    }

    if (Row_Bonus_Text)
        Row_Bonus_Text->SetText(FText::FromString(BuildBonusSummary(Entry.bonuses)));

    if (Row_Equip_Button)
        Row_Equip_Button->SetIsEnabled(true);

    if (Row_Equip_Button_Text)
        Row_Equip_Button_Text->SetText(FText::FromString(
            bIsEquipped ? TEXT("Unequip") : TEXT("Equip")));
}

void UTitleRowWidget::HandleEquipClicked()
{
    // Toggle: equipped → send empty slug to unequip, not equipped → send slug to equip
    OnEquipRequested.Broadcast(bRowIsEquipped ? FString() : RowSlug);
}

FString UTitleRowWidget::BuildBonusSummary(const TArray<FTitleBonusEntry>& Bonuses)
{
    TArray<FString> Parts;
    Parts.Reserve(Bonuses.Num());
    for (const FTitleBonusEntry& B : Bonuses)
    {
        const FString Sign = B.value >= 0.0f ? TEXT("+") : TEXT("");
        FString AttrName = B.attributeSlug;
        AttrName.ReplaceInline(TEXT("_"), TEXT(" "));
        if (AttrName.Len() > 0)
        {
            AttrName[0] = FChar::ToUpper(AttrName[0]);
        }
        Parts.Add(FString::Printf(TEXT("%s%.0f %s"), *Sign, B.value, *AttrName));
    }
    return FString::Join(Parts, TEXT(", "));
}
