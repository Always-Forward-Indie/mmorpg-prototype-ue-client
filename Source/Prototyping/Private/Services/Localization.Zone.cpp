#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

FText ULocalizationSubsystem::GetZoneDisplayName(const FString& ZoneSlug) const
{
    if (ZoneSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetZoneLocaleTable();
    if (!Table) return FallbackText(ZoneSlug);
    const FZoneLocaleDefinition* Row = Table->FindRow<FZoneLocaleDefinition>(FName(*ZoneSlug), TEXT("GetZoneDisplayName"), false);
    return (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(ZoneSlug);
}

FText ULocalizationSubsystem::GetZoneDescription(const FString& ZoneSlug) const
{
    if (ZoneSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetZoneLocaleTable();
    if (!Table) return FText::GetEmpty();
    const FZoneLocaleDefinition* Row = Table->FindRow<FZoneLocaleDefinition>(FName(*ZoneSlug), TEXT("GetZoneDescription"), false);
    return Row ? Row->description : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetZoneLocaleDefinition(const FString& ZoneSlug, FZoneLocaleDefinition& OutDefinition) const
{
    if (ZoneSlug.IsEmpty()) return false;
    UDataTable* Table = GetZoneLocaleTable();
    if (!Table) return false;
    const FZoneLocaleDefinition* Row = Table->FindRow<FZoneLocaleDefinition>(FName(*ZoneSlug), TEXT("GetZoneLocaleDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? World Notifications ???????????????????????????????????????????????????????

