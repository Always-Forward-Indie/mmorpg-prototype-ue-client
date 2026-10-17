#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

FText ULocalizationSubsystem::GetNPCDisplayName(const FString& NPCSlug) const
{
    if (NPCSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetNPCLocaleTable();
    if (Table)
    {
        const FNPCLocaleDefinition* Row = Table->FindRow<FNPCLocaleDefinition>(FName(*NPCSlug), TEXT("GetNPCDisplayName"));
        if (Row && !Row->displayName.IsEmpty()) return Row->displayName;
    }
    return FallbackText(NPCSlug);
}

// ?? Mobs ??????????????????????????????????????????????????????????????????????

FText ULocalizationSubsystem::GetMobDisplayName(const FString& MobSlug) const
{
    if (MobSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetMobLocaleTable();
    if (!Table) return FallbackText(MobSlug);
    const FMobLocaleDefinition* Row = Table->FindRow<FMobLocaleDefinition>(FName(*MobSlug), TEXT("GetMobDisplayName"), false);
    return (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(MobSlug);
}

FText ULocalizationSubsystem::GetMobDescription(const FString& MobSlug) const
{
    if (MobSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetMobLocaleTable();
    if (!Table) return FText::GetEmpty();
    const FMobLocaleDefinition* Row = Table->FindRow<FMobLocaleDefinition>(FName(*MobSlug), TEXT("GetMobDescription"), false);
    return Row ? Row->description : FText::GetEmpty();
}

FText ULocalizationSubsystem::GetMobLoreText(const FString& LoreKey) const
{
    if (LoreKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetMobLocaleTable();
    if (!Table) return FText::GetEmpty();
    const FMobLocaleDefinition* Row = Table->FindRow<FMobLocaleDefinition>(FName(*LoreKey), TEXT("GetMobLoreText"), false);
    return Row ? Row->loreText : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetMobLocaleDefinition(const FString& MobSlug, FMobLocaleDefinition& OutDefinition) const
{
    if (MobSlug.IsEmpty()) return false;
    UDataTable* Table = GetMobLocaleTable();
    if (!Table) return false;
    const FMobLocaleDefinition* Row = Table->FindRow<FMobLocaleDefinition>(FName(*MobSlug), TEXT("GetMobLocaleDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Bestiary Categories ???????????????????????????????????????????????????????

