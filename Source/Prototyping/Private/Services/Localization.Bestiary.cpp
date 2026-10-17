#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

FText ULocalizationSubsystem::GetBestiaryCategoryName(const FString& CategorySlug) const
{
    if (CategorySlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetBestiaryCategoryTable();
    if (!Table) return FallbackText(CategorySlug);
    const FBestiaryCategoryDefinition* Row = Table->FindRow<FBestiaryCategoryDefinition>(FName(*CategorySlug), TEXT("GetBestiaryCategoryName"), false);
    return (Row && !Row->categoryTitle.IsEmpty()) ? Row->categoryTitle : FallbackText(CategorySlug);
}

FText ULocalizationSubsystem::GetBestiaryCategoryLockedHint(const FString& CategorySlug) const
{
    if (CategorySlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetBestiaryCategoryTable();
    if (!Table) return FText::GetEmpty();
    const FBestiaryCategoryDefinition* Row = Table->FindRow<FBestiaryCategoryDefinition>(FName(*CategorySlug), TEXT("GetBestiaryCategoryLockedHint"), false);
    return Row ? Row->lockedHint : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetBestiaryCategoryDefinition(const FString& CategorySlug, FBestiaryCategoryDefinition& OutDefinition) const
{
    if (CategorySlug.IsEmpty()) return false;
    UDataTable* Table = GetBestiaryCategoryTable();
    if (!Table) return false;
    const FBestiaryCategoryDefinition* Row = Table->FindRow<FBestiaryCategoryDefinition>(FName(*CategorySlug), TEXT("GetBestiaryCategoryDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Zones ?????????????????????????????????????????????????????????????????????

