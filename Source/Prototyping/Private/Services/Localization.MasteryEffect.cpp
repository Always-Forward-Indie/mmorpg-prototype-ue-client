#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

UDataTable* ULocalizationSubsystem::GetMasteryLocaleTable() const
{
    if (!CachedMasteryLocaleTable && LocalizationData)
        CachedMasteryLocaleTable = LocalizationData->MasteryLocale.Get();

    UE_LOG(LogTemp, Warning, TEXT("[LOCALE] GetMasteryLocaleTable: LocalizationData=%s Table=%s"),
        LocalizationData ? TEXT("valid") : TEXT("NULL"),
        CachedMasteryLocaleTable ? *CachedMasteryLocaleTable->GetName() : TEXT("NULL"));

    return CachedMasteryLocaleTable;
}

FText ULocalizationSubsystem::GetMasteryDisplayName(const FString& MasterySlug) const
{
    if (MasterySlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetMasteryLocaleTable();
    if (!Table) return FallbackText(MasterySlug);
    const FMasteryLocaleDefinition* Row = Table->FindRow<FMasteryLocaleDefinition>(
        FName(*MasterySlug), TEXT("GetMasteryDisplayName"), false);
    const FText Result = (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(MasterySlug);

    UE_LOG(LogTemp, Warning, TEXT("[LOCALE] GetMasteryDisplayName('%s'): Row=%s Result='%s'"),
        *MasterySlug, Row ? TEXT("found") : TEXT("NULL"), *Result.ToString());

    return Result;
}

FText ULocalizationSubsystem::GetMasteryDescription(const FString& MasterySlug) const
{
    if (MasterySlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetMasteryLocaleTable();
    if (!Table) return FallbackText(MasterySlug);
    const FMasteryLocaleDefinition* Row = Table->FindRow<FMasteryLocaleDefinition>(
        FName(*MasterySlug), TEXT("GetMasteryDescription"), false);
    return (Row && !Row->description.IsEmpty()) ? Row->description : FText::GetEmpty();
}

UDataTable* ULocalizationSubsystem::GetEffectLocaleTable() const
{
    if (!CachedEffectLocaleTable && LocalizationData)
        CachedEffectLocaleTable = LocalizationData->EffectLocale.Get();

    UE_LOG(LogTemp, Warning, TEXT("[LOCALE] GetEffectLocaleTable: LocalizationData=%s Table=%s"),
        LocalizationData ? TEXT("valid") : TEXT("NULL"),
        CachedEffectLocaleTable ? *CachedEffectLocaleTable->GetName() : TEXT("NULL"));

    return CachedEffectLocaleTable;
}

FText ULocalizationSubsystem::GetEffectDisplayName(const FString& EffectSlug) const
{
    if (EffectSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetEffectLocaleTable();
    if (!Table) return FallbackText(EffectSlug);
    const FEffectLocaleDefinition* Row = Table->FindRow<FEffectLocaleDefinition>(
        FName(*EffectSlug), TEXT("GetEffectDisplayName"), false);
    const FText Result = (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(EffectSlug);

    UE_LOG(LogTemp, Warning, TEXT("[LOCALE] GetEffectDisplayName('%s'): Row=%s Result='%s'"),
        *EffectSlug, Row ? TEXT("found") : TEXT("NULL"), *Result.ToString());

    return Result;
}

FText ULocalizationSubsystem::GetEffectDescription(const FString& EffectSlug) const
{
    if (EffectSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetEffectLocaleTable();
    if (!Table) return FText::GetEmpty();
    const FEffectLocaleDefinition* Row = Table->FindRow<FEffectLocaleDefinition>(
        FName(*EffectSlug), TEXT("GetEffectDescription"), false);
    return (Row && !Row->description.IsEmpty()) ? Row->description : FText::GetEmpty();
}
