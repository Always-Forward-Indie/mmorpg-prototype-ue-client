#include "Services/LocalizationSubsystem.h"
#include "Prototyping.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

UDataTable* ULocalizationSubsystem::GetWIOLocaleTable() const
{
    if (!CachedWIOLocaleTable && LocalizationData)
        CachedWIOLocaleTable = LocalizationData->WorldObjectLocale.Get();
    return CachedWIOLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetTitleLocaleTable() const
{
    if (!CachedTitleLocaleTable && LocalizationData)
        CachedTitleLocaleTable = LocalizationData->TitleLocale.Get();

    UE_LOG(LogGameplay, Verbose, TEXT("[LOCALE] GetTitleLocaleTable: LocalizationData=%s Table=%s"),
        LocalizationData ? TEXT("valid") : TEXT("NULL"),
        CachedTitleLocaleTable ? *CachedTitleLocaleTable->GetName() : TEXT("NULL"));

    return CachedTitleLocaleTable;
}

FText ULocalizationSubsystem::GetWIODisplayName(const FString& NameKey) const
{
    if (NameKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetWIOLocaleTable();
    if (!Table) return FallbackText(NameKey);
    const FWIOLocaleDefinition* Row = Table->FindRow<FWIOLocaleDefinition>(
        FName(*NameKey), TEXT("GetWIODisplayName"), false);
    return (Row && !Row->DisplayName.IsEmpty()) ? Row->DisplayName : FallbackText(NameKey);
}

FText ULocalizationSubsystem::GetWIODescription(const FString& NameKey) const
{
    if (NameKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetWIOLocaleTable();
    if (!Table) return FallbackText(NameKey);
    const FWIOLocaleDefinition* Row = Table->FindRow<FWIOLocaleDefinition>(
        FName(*NameKey), TEXT("GetWIODescription"), false);
    return (Row && !Row->Description.IsEmpty()) ? Row->Description : FallbackText(NameKey);
}

FText ULocalizationSubsystem::GetWIOInteractionPrompt(const FString& NameKey) const
{
    if (NameKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetWIOLocaleTable();
    if (!Table) return FallbackText(NameKey);
    const FWIOLocaleDefinition* Row = Table->FindRow<FWIOLocaleDefinition>(
        FName(*NameKey), TEXT("GetWIOInteractionPrompt"), false);
    return (Row && !Row->InteractionPrompt.IsEmpty()) ? Row->InteractionPrompt : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetWIOLocaleDefinition(const FString& NameKey, FWIOLocaleDefinition& OutDefinition) const
{
    if (NameKey.IsEmpty()) return false;
    UDataTable* Table = GetWIOLocaleTable();
    if (!Table) return false;
    const FWIOLocaleDefinition* Row = Table->FindRow<FWIOLocaleDefinition>(
        FName(*NameKey), TEXT("GetWIOLocaleDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// -- Titles ------------------------------------------------------------------

