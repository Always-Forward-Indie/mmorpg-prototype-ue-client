#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Utils/JSONParser.h"

// S3a codebook statics: attribute / item-type / rarity / equip-slot / item-set
// display names keyed by slug. Miss (no table, no row, empty row) falls back
// to a humanised slug — renders exactly like the pre-S3a inline hacks until
// content lands in S3b.

FText ULocalizationSubsystem::GetAttributeDisplayName(const FString& AttributeSlug) const
{
    if (AttributeSlug.IsEmpty()) return FText::GetEmpty();
    if (UDataTable* Table = GetAttributeLocaleTable())
    {
        if (const FAttributeLocaleDefinition* Row =
                Table->FindRow<FAttributeLocaleDefinition>(FName(*AttributeSlug), TEXT("GetAttributeDisplayName"), false))
        {
            if (Row && !Row->displayName.IsEmpty())
                return Row->displayName;
        }
    }
    return FText::FromString(JSONParser::HumanizeSlugForDisplay(AttributeSlug));
}

FText ULocalizationSubsystem::GetItemTypeDisplayName(const FString& ItemTypeSlug) const
{
    if (ItemTypeSlug.IsEmpty()) return FText::GetEmpty();
    if (UDataTable* Table = GetItemTypeLocaleTable())
    {
        if (const FItemTypeLocaleDefinition* Row =
                Table->FindRow<FItemTypeLocaleDefinition>(FName(*ItemTypeSlug), TEXT("GetItemTypeDisplayName"), false))
        {
            if (Row && !Row->displayName.IsEmpty())
                return Row->displayName;
        }
    }
    return FText::FromString(JSONParser::HumanizeSlugForDisplay(ItemTypeSlug));
}

FText ULocalizationSubsystem::GetRarityDisplayName(const FString& RaritySlug) const
{
    if (RaritySlug.IsEmpty()) return FText::GetEmpty();
    if (UDataTable* Table = GetRarityLocaleTable())
    {
        if (const FRarityLocaleDefinition* Row =
                Table->FindRow<FRarityLocaleDefinition>(FName(*RaritySlug), TEXT("GetRarityDisplayName"), false))
        {
            if (Row && !Row->displayName.IsEmpty())
                return Row->displayName;
        }
    }
    return FText::FromString(JSONParser::HumanizeSlugForDisplay(RaritySlug));
}

FText ULocalizationSubsystem::GetEquipSlotDisplayName(const FString& EquipSlotSlug) const
{
    if (EquipSlotSlug.IsEmpty()) return FText::GetEmpty();
    if (UDataTable* Table = GetEquipSlotLocaleTable())
    {
        if (const FEquipSlotLocaleDefinition* Row =
                Table->FindRow<FEquipSlotLocaleDefinition>(FName(*EquipSlotSlug), TEXT("GetEquipSlotDisplayName"), false))
        {
            if (Row && !Row->displayName.IsEmpty())
                return Row->displayName;
        }
    }
    return FText::FromString(JSONParser::HumanizeSlugForDisplay(EquipSlotSlug));
}

FText ULocalizationSubsystem::GetItemSetDisplayName(const FString& ItemSetSlug) const
{
    if (ItemSetSlug.IsEmpty()) return FText::GetEmpty();
    if (UDataTable* Table = GetItemSetLocaleTable())
    {
        if (const FItemSetLocaleDefinition* Row =
                Table->FindRow<FItemSetLocaleDefinition>(FName(*ItemSetSlug), TEXT("GetItemSetDisplayName"), false))
        {
            if (Row && !Row->displayName.IsEmpty())
                return Row->displayName;
        }
    }
    return FText::FromString(JSONParser::HumanizeSlugForDisplay(ItemSetSlug));
}
