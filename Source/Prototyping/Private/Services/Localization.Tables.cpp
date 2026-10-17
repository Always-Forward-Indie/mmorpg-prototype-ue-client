#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

UDataTable* ULocalizationSubsystem::GetQuestTable() const
{
    if (!CachedQuestTable && LocalizationData)
        CachedQuestTable = LocalizationData->QuestDefinitions.Get();
    return CachedQuestTable;
}

UDataTable* ULocalizationSubsystem::GetQuestStepTable() const
{
    if (!CachedQuestStepTable && LocalizationData)
        CachedQuestStepTable = LocalizationData->QuestStepDefinitions.Get();
    return CachedQuestStepTable;
}

UDataTable* ULocalizationSubsystem::GetDialogueNodeTable() const
{
    if (!CachedDialogueNodeTable && LocalizationData)
        CachedDialogueNodeTable = LocalizationData->DialogueNodes.Get();
    return CachedDialogueNodeTable;
}

UDataTable* ULocalizationSubsystem::GetDialogueChoiceTable() const
{
    if (!CachedDialogueChoiceTable && LocalizationData)
        CachedDialogueChoiceTable = LocalizationData->DialogueChoices.Get();
    return CachedDialogueChoiceTable;
}

UDataTable* ULocalizationSubsystem::GetItemLocaleTable() const
{
    if (!CachedItemLocaleTable && LocalizationData)
        CachedItemLocaleTable = LocalizationData->ItemLocale.Get();
    return CachedItemLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetMobLocaleTable() const
{
    if (!CachedMobLocaleTable && LocalizationData)
        CachedMobLocaleTable = LocalizationData->MobLocale.Get();
    return CachedMobLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetNPCLocaleTable() const
{
    if (!CachedNPCLocaleTable && LocalizationData)
        CachedNPCLocaleTable = LocalizationData->NPCLocale.Get();
    return CachedNPCLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetBestiaryCategoryTable() const
{
    if (!CachedBestiaryCategoryTable && LocalizationData)
        CachedBestiaryCategoryTable = LocalizationData->BestiaryCategories.Get();
    return CachedBestiaryCategoryTable;
}

UDataTable* ULocalizationSubsystem::GetZoneLocaleTable() const
{
    if (!CachedZoneLocaleTable && LocalizationData)
        CachedZoneLocaleTable = LocalizationData->ZoneLocale.Get();
    return CachedZoneLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetNotificationLocaleTable() const
{
    if (!CachedNotificationLocaleTable && LocalizationData)
        CachedNotificationLocaleTable = LocalizationData->NotificationLocale.Get();
    return CachedNotificationLocaleTable;
}

FText ULocalizationSubsystem::FallbackText(const FString& Key)
{
    return Key.IsEmpty() ? FText::GetEmpty() : FText::FromString(Key);
}

// ?? Quests ????????????????????????????????????????????????????????????????????

