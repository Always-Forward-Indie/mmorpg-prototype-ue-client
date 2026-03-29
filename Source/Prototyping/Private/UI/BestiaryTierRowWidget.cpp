#include "UI/BestiaryTierRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UBestiaryTierRowWidget::Setup(const FBestiaryTierStruct& Tier)
{
    // Cache localization subsystem once
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        Loc = GI->GetSubsystem<ULocalizationSubsystem>();

    // --- Category label ---
    if (Tier_Category_Text)
    {
        FText CatName = Loc
            ? Loc->GetBestiaryCategoryName(Tier.categorySlug)
            : FText::FromString(Tier.categorySlug);
        Tier_Category_Text->SetText(CatName);
    }

    if (Tier.unlocked)
    {
        // Hide locked hint
        if (Tier_Locked_Text)
            Tier_Locked_Text->SetVisibility(ESlateVisibility::Collapsed);

        // Show and fill content box
        if (Tier_Content_Box)
        {
            Tier_Content_Box->ClearChildren();
            Tier_Content_Box->SetVisibility(ESlateVisibility::Visible);
            PopulateTierContent(Tier);
        }

        // Show unlocked icon
        if (Tier_Status_Icon)
            Tier_Status_Icon->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        // Hide content
        if (Tier_Content_Box)
            Tier_Content_Box->SetVisibility(ESlateVisibility::Collapsed);

        // Hide unlocked icon
        if (Tier_Status_Icon)
            Tier_Status_Icon->SetVisibility(ESlateVisibility::Collapsed);

        // Build locked hint text
        if (Tier_Locked_Text)
        {
            FText LockedHint = Loc
                ? Loc->GetBestiaryCategoryLockedHint(Tier.categorySlug)
                : FText::GetEmpty();

            FString LockedStr;
            if (!LockedHint.IsEmpty())
            {
                LockedStr = Tier.requiredKillsLeft > 0
                    ? FString::Printf(TEXT("%s (%d)"), *LockedHint.ToString(), Tier.requiredKillsLeft)
                    : LockedHint.ToString();
            }
            else
            {
                LockedStr = Tier.requiredKillsLeft > 0
                    ? FString::Printf(TEXT("??? \u2014 %d more kills"), Tier.requiredKillsLeft)
                    : FString::Printf(TEXT("??? \u2014 %d kills needed"), Tier.requiredKills);
            }

            Tier_Locked_Text->SetText(FText::FromString(LockedStr));
            Tier_Locked_Text->SetVisibility(ESlateVisibility::Visible);
        }
    }

    OnSetupComplete(Tier);
}

void UBestiaryTierRowWidget::AddLine(const FString& Line)
{
    if (!Tier_Content_Box) return;
    UTextBlock* Txt = NewObject<UTextBlock>(Tier_Content_Box);
    Txt->SetText(FText::FromString(Line));
    Tier_Content_Box->AddChild(Txt);
}

void UBestiaryTierRowWidget::PopulateTierContent(const FBestiaryTierStruct& Tier)
{
    if (Tier.categorySlug == TEXT("basic_info"))
    {
        AddLine(FString::Printf(TEXT("%d"), Tier.level));
        AddLine(Tier.rank);
        AddLine(FString::Printf(TEXT("%d \u2014 %d"), Tier.hpMin, Tier.hpMax));
        AddLine(Tier.mobType);
        AddLine(Tier.biomeSlug);
    }
    else if (Tier.categorySlug == TEXT("lore"))
    {
        FText LoreText = Loc
            ? Loc->GetMobLoreText(Tier.loreKey)
            : FText::FromString(Tier.loreKey);
        AddLine(LoreText.ToString());
    }
    else if (Tier.categorySlug == TEXT("combat_info"))
    {
        if (Tier.weaknesses.Num() > 0)
            AddLine(FString::Join(Tier.weaknesses, TEXT(", ")));
        if (Tier.resistances.Num() > 0)
            AddLine(FString::Join(Tier.resistances, TEXT(", ")));
        if (Tier.abilities.Num() > 0)
            AddLine(FString::Join(Tier.abilities, TEXT(", ")));
    }
    else if (Tier.categorySlug == TEXT("loot_table"))
    {
        for (const FString& ItemSlug : Tier.lootItems)
        {
            FText ItemName = Loc
                ? Loc->GetItemDisplayName(ItemSlug)
                : FText::FromString(ItemSlug);
            AddLine(ItemName.ToString());
        }
    }
    else if (Tier.categorySlug == TEXT("drop_rates"))
    {
        for (const FBestiaryLootEntryStruct& Entry : Tier.loot)
        {
            FText ItemName = Loc
                ? Loc->GetItemDisplayName(Entry.itemSlug)
                : FText::FromString(Entry.itemSlug);
            AddLine(FString::Printf(TEXT("%s \u2014 %.2f%%"), *ItemName.ToString(), Entry.chance));
        }
    }
    else if (Tier.categorySlug == TEXT("hunter_mastery"))
    {
        if (!Tier.titleSlug.IsEmpty())
            AddLine(Tier.titleSlug);
        if (!Tier.achievementSlug.IsEmpty())
            AddLine(Tier.achievementSlug);
    }
}
