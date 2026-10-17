#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

FText ULocalizationSubsystem::GetItemDisplayName(const FString& ItemSlug) const
{
    if (ItemSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetItemLocaleTable();
    if (!Table) return FallbackText(ItemSlug);
    const FItemLocaleDefinition* Row = Table->FindRow<FItemLocaleDefinition>(FName(*ItemSlug), TEXT("GetItemDisplayName"), false);
    return (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(ItemSlug);
}

FText ULocalizationSubsystem::GetItemDescription(const FString& ItemSlug) const
{
    if (ItemSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetItemLocaleTable();
    if (!Table) return FText::GetEmpty();
    const FItemLocaleDefinition* Row = Table->FindRow<FItemLocaleDefinition>(FName(*ItemSlug), TEXT("GetItemDescription"), false);
    return Row ? Row->description : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetItemLocaleDefinition(const FString& ItemSlug, FItemLocaleDefinition& OutDefinition) const
{
    if (ItemSlug.IsEmpty()) return false;
    UDataTable* Table = GetItemLocaleTable();
    if (!Table) return false;
    const FItemLocaleDefinition* Row = Table->FindRow<FItemLocaleDefinition>(FName(*ItemSlug), TEXT("GetItemLocaleDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? NPCs ??????????????????????????????????????????????????????????????????????

