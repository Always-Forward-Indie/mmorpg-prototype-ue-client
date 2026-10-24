#include "UI/VendorTooltipWidget.h"
#include "Data/DataStructs.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Utils/JSONParser.h"
#include "Engine/AssetManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UVendorTooltipWidget::ApplyCommonFields(const FString& Name, const FString& TypeName,
const FString& RarityName, const FString& RaritySlug, const FString& Description,
float Weight, int32 DurabilityMax, int32 DurabilityCurrent,
bool bIsDurable, const FString& Slug,
int32 LevelRequirement,
bool bIsEquippable,
const FString& EquipSlotName,
bool bIsTwoHanded,
bool bIsQuestItem,
bool bIsTradable,
const TMap<FString, FString>& Attributes,
const TArray<FItemUseEffectEntry>& UseEffects,
const FString& MasterySlug,
int32 KillCount)
{
    // Resolve localised name and description via LocalizationSubsystem when a slug is available
    // Pretty-print the raw server name/slug as fallback (replace underscores, capitalise first letter)
    auto PrettyFallback = [](const FString& Raw) -> FText
    {
        return FText::FromString(JSONParser::HumanizeSlugForDisplay(Raw));
    };

    ULocalizationSubsystem* Loc = ULocalizationSubsystem::Get(this);
    FText LocalisedName        = PrettyFallback(Name.IsEmpty() ? Slug : Name);
    FText LocalisedDescription = FText::FromString(Description);
    if (!Slug.IsEmpty() && Loc)
    {
        const FText LocName = Loc->GetItemDisplayName(Slug);
        if (!LocName.IsEmpty()) LocalisedName = LocName;
        const FText LocDesc = Loc->GetItemDescription(Slug);
        if (!LocDesc.IsEmpty()) LocalisedDescription = LocDesc;
    }

    if (ItemNameText)
    {
        ItemNameText->SetText(LocalisedName);
        ItemNameText->SetColorAndOpacity(GetRarityColor(RaritySlug));
    }

    // S3a: TypeName/RarityName/EquipSlotName params carry slugs (callers pass
    // itemTypeSlug/raritySlug/equipSlotSlug); resolve via codebook tables.
    if (ItemTypeText)
    {
        FText DisplayType = FText::GetEmpty();
        if (!TypeName.IsEmpty() && Loc)
            DisplayType = Loc->GetItemTypeDisplayName(TypeName);
        if (DisplayType.IsEmpty())
            DisplayType = PrettyFallback(TypeName);
        if (!DisplayType.IsEmpty())
        {
            ItemTypeText->SetText(DisplayType);
            ItemTypeText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ItemTypeText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (ItemTypeText || LevelRequirementText)
    {
        const bool bHasTypeOrLevelReq = !TypeName.IsEmpty() || LevelRequirement > 1;

        if (!bHasTypeOrLevelReq)
        {
            if (Separator2) Separator2->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            if (Separator2) Separator2->SetVisibility(ESlateVisibility::Visible);
        }
    }

    if (ItemRarityText)
    {
        // S3a: RarityName param is usually empty (S2.4 slim); RaritySlug rules.
        const FString& RarityKey = RarityName.IsEmpty() ? RaritySlug : RarityName;
        FText DisplayRarity = FText::GetEmpty();
        if (!RarityKey.IsEmpty() && Loc && RarityName.IsEmpty())
            DisplayRarity = Loc->GetRarityDisplayName(RarityKey);
        if (DisplayRarity.IsEmpty())
            DisplayRarity = RarityName.IsEmpty() ? PrettyFallback(RarityKey) : FText::FromString(RarityName);
        if (!DisplayRarity.IsEmpty())
        {
            ItemRarityText->SetText(DisplayRarity);
            ItemRarityText->SetColorAndOpacity(GetRarityColor(RaritySlug));
            ItemRarityText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ItemRarityText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (ItemDescriptionText)
    {
        if (!LocalisedDescription.IsEmpty())
        {
            ItemDescriptionText->SetText(LocalisedDescription);
            ItemDescriptionText->SetVisibility(ESlateVisibility::Visible);
            if (Separator4) Separator4->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ItemDescriptionText->SetVisibility(ESlateVisibility::Collapsed);
            if (Separator4) Separator4->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (WeightText)
    {
        if (Weight > 0.f)
        {
            WeightText->SetText(FText::FromString(FString::Printf(TEXT("Weight: %.1f kg"), Weight)));
            WeightText->SetVisibility(ESlateVisibility::Visible);
            if (Separator6) Separator6->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            WeightText->SetVisibility(ESlateVisibility::Collapsed);
            if (Separator6) Separator6->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (DurabilityText)
    {
        if (bIsDurable)
        {
            const float Ratio = (DurabilityMax > 0)
                ? static_cast<float>(DurabilityCurrent) / static_cast<float>(DurabilityMax)
                : 1.f;
            const FLinearColor DurColor = (Ratio <= 0.25f) ? FLinearColor::Red
                : (Ratio <= 0.5f) ? FLinearColor::Yellow
                : FLinearColor(0.6f, 0.9f, 0.6f, 1.f);
            DurabilityText->SetText(FText::FromString(
                FString::Printf(TEXT("Durability: %d / %d"), DurabilityCurrent, DurabilityMax)));
            DurabilityText->SetColorAndOpacity(DurColor);
            DurabilityText->SetVisibility(ESlateVisibility::Visible);
			if (Separator3) Separator3->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            DurabilityText->SetVisibility(ESlateVisibility::Collapsed);
            if (Separator3) Separator3->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Equip slot (S3a: EquipSlotName param carries the slug; codebook lookup).
    if (EquipSlotText)
    {
        if (bIsEquippable && !EquipSlotName.IsEmpty())
        {
            FText SlotLabel = FText::GetEmpty();
            if (Loc)
                SlotLabel = Loc->GetEquipSlotDisplayName(EquipSlotName);
            if (SlotLabel.IsEmpty())
                SlotLabel = PrettyFallback(EquipSlotName);
            FString SlotStr = SlotLabel.ToString();
            if (bIsTwoHanded) SlotStr += TEXT(" (Two-Handed)");
            EquipSlotText->SetText(FText::FromString(SlotStr));
            EquipSlotText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            EquipSlotText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Level requirement
    if (LevelRequirementText)
    {
        if (LevelRequirement > 1)
        {
            LevelRequirementText->SetText(FText::FromString(
                FString::Printf(TEXT("Required Level: %d"), LevelRequirement)));
            LevelRequirementText->SetColorAndOpacity(FLinearColor(1.f, 0.85f, 0.45f));
            LevelRequirementText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            LevelRequirementText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Flags
    if (FlagsText)
    {
        TArray<FString> Flags;
        if (bIsQuestItem)  Flags.Add(TEXT("Quest Item"));
        if (!bIsTradable)  Flags.Add(TEXT("Not Tradable"));
        if (Flags.Num() > 0)
        {
            FlagsText->SetText(FText::FromString(FString::Join(Flags, TEXT("  \u00b7  "))));
            FlagsText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            FlagsText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Attributes
    if (AttributesText)
    {
        if (Attributes.Num() > 0)
        {
            TArray<FString> Lines;
            for (const auto& Pair : Attributes)
            {
                // S3a: key may be a server name, a slug, or (S2.4) already resolved.
                // Prefer the codebook table for slug keys; keep server names verbatim.
                FString AttrName = Pair.Key;
                if (Loc && !AttrName.IsEmpty() && AttrName.Find(TEXT(" ")) == INDEX_NONE)
                {
                    const FText LocAttr = Loc->GetAttributeDisplayName(AttrName);
                    if (!LocAttr.IsEmpty() && LocAttr.ToString() != AttrName)
                        AttrName = LocAttr.ToString();
                    else
                        AttrName = PrettyFallback(AttrName).ToString();
                }
                Lines.Add(FString::Printf(TEXT("%s: %s"), *AttrName, *Pair.Value));
            }
            AttributesText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
            AttributesText->SetColorAndOpacity(FLinearColor(0.75f, 0.95f, 1.f));
            AttributesText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            AttributesText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Use effects
    if (UseEffectText)
    {
        if (UseEffects.Num() > 0)
        {
            TArray<FString> EffLines;
            for (const FItemUseEffectEntry& Effect : UseEffects)
            {
                // S3a: codebook lookup for the target slug.
                FString AttrLabel = Effect.attributeSlug;
                if (Loc && !AttrLabel.IsEmpty())
                    AttrLabel = Loc->GetAttributeDisplayName(Effect.attributeSlug).ToString();
                if (AttrLabel.IsEmpty())
                    AttrLabel = PrettyFallback(Effect.attributeSlug).ToString();
                FString Line;
                if (Effect.isInstant)
                    Line = FString::Printf(TEXT("Use: Restores %.0f %s"), Effect.value, *AttrLabel);
                else if (Effect.durationSeconds > 0)
                    Line = FString::Printf(TEXT("Use: +%.0f %s for %ds"), Effect.value, *AttrLabel, Effect.durationSeconds);
                else
                    Line = FString::Printf(TEXT("Use: %.0f %s"), Effect.value, *AttrLabel);
                if (Effect.cooldownSeconds > 0)
                    Line += FString::Printf(TEXT(" (%ds cooldown)"), Effect.cooldownSeconds);
                EffLines.Add(Line);
            }
            UseEffectText->SetText(FText::FromString(FString::Join(EffLines, TEXT("\n"))));
            UseEffectText->SetColorAndOpacity(FLinearColor(0.4f, 1.f, 0.4f));
            UseEffectText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            UseEffectText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if(UseEffects.Num() == 0 && Attributes.Num() == 0)
    {
        if (Separator5) Separator5->SetVisibility(ESlateVisibility::Collapsed);
	}
    else
    {
        if (Separator5) Separator5->SetVisibility(ESlateVisibility::Visible);
    }

    // Weapon soul (mastery + kill count)
    if (WeaponSoulText)
    {
        const bool bHasMastery = !MasterySlug.IsEmpty();
        const bool bHasKills   = KillCount > 0;
        if (bHasMastery || bHasKills)
        {
            TArray<FString> Parts;
            if (bHasMastery)
            {
                // S3a: mastery display name from its locale table.
                FString ML = MasterySlug;
                if (Loc && !MasterySlug.IsEmpty())
                {
                    const FText LocMastery = Loc->GetMasteryDisplayName(MasterySlug);
                    if (!LocMastery.IsEmpty())
                        ML = LocMastery.ToString();
                    else
                        ML = PrettyFallback(MasterySlug).ToString();
                }
                Parts.Add(FString::Printf(TEXT("Mastery: %s"), *ML));
            }
            if (bHasKills)
                Parts.Add(FString::Printf(TEXT("Kills: %d"), KillCount));
            WeaponSoulText->SetText(FText::FromString(FString::Join(Parts, TEXT("  |  "))));
            WeaponSoulText->SetColorAndOpacity(FLinearColor(1.f, 0.6f, 0.1f));
            WeaponSoulText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            WeaponSoulText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    LoadIconBySlug(Slug);
}

