#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

FText ULocalizationSubsystem::GetTitleDisplayName(const FString& TitleSlug) const
{
    if (TitleSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetTitleLocaleTable();
    if (!Table) return FallbackText(TitleSlug);
    const FTitleLocaleDefinition* Row = Table->FindRow<FTitleLocaleDefinition>(
        FName(*TitleSlug), TEXT("GetTitleDisplayName"), false);
    const FText Result = (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(TitleSlug);

    UE_LOG(LogTemp, Warning, TEXT("[LOCALE] GetTitleDisplayName('%s'): Row=%s Result='%s'"),
        *TitleSlug, Row ? TEXT("found") : TEXT("NULL"), *Result.ToString());

    return Result;
}

FText ULocalizationSubsystem::GetTitleDescription(const FString& TitleSlug) const
{
    if (TitleSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetTitleLocaleTable();
    if (!Table) return FText::GetEmpty();
    const FTitleLocaleDefinition* Row = Table->FindRow<FTitleLocaleDefinition>(
        FName(*TitleSlug), TEXT("GetTitleDescription"), false);
    return (Row && !Row->description.IsEmpty()) ? Row->description : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetTitleLocaleDefinition(const FString& TitleSlug, FTitleLocaleDefinition& OutDefinition) const
{
    if (TitleSlug.IsEmpty()) return false;
    UDataTable* Table = GetTitleLocaleTable();
    if (!Table) return false;
    const FTitleLocaleDefinition* Row = Table->FindRow<FTitleLocaleDefinition>(
        FName(*TitleSlug), TEXT("GetTitleLocaleDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

