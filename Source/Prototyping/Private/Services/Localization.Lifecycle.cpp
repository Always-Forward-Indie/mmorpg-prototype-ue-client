#include "Services/LocalizationSubsystem.h"
#include "Prototyping.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

void ULocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void ULocalizationSubsystem::SetLocalizationData(ULocalizationDataAsset* InAsset)
{
    LocalizationData = InAsset;

    CachedQuestTable              = nullptr;
    CachedQuestStepTable          = nullptr;
    CachedDialogueNodeTable       = nullptr;
    CachedDialogueChoiceTable     = nullptr;
    CachedItemLocaleTable         = nullptr;
    CachedMobLocaleTable          = nullptr;
    CachedNPCLocaleTable          = nullptr;
    CachedBestiaryCategoryTable   = nullptr;
    CachedZoneLocaleTable         = nullptr;
    CachedNotificationLocaleTable = nullptr;
    CachedAmbientSpeechTable      = nullptr;
    CachedWIOLocaleTable          = nullptr;
    CachedTitleLocaleTable        = nullptr;
    CachedMasteryLocaleTable      = nullptr;
    CachedEffectLocaleTable       = nullptr;
    CachedAttributeLocaleTable    = nullptr;
    CachedItemTypeLocaleTable     = nullptr;
    CachedRarityLocaleTable       = nullptr;
    CachedEquipSlotLocaleTable    = nullptr;
    CachedItemSetLocaleTable      = nullptr;
    CachedSkillLocaleTable        = nullptr;
    CachedEmoteLocaleTable        = nullptr;
    CachedClassLocaleTable        = nullptr;
    CachedRaceLocaleTable         = nullptr;
    CachedGenderLocaleTable       = nullptr;

    UE_LOG(LogGameplay, Log, TEXT("LocalizationSubsystem: data asset %s"),
        InAsset ? TEXT("assigned") : TEXT("cleared"));
}

void ULocalizationSubsystem::SetLocale(const FString& InLocale)
{
    if (CurrentLocale.Equals(InLocale, ESearchCase::IgnoreCase)) return;

    const bool bIsRussian = InLocale.StartsWith(TEXT("ru"), ESearchCase::IgnoreCase);
    ULocalizationDataAsset* Target = bIsRussian ? LocalizationDataRU : LocalizationDataEN;

    if (!Target)
    {
        UE_LOG(LogGameplay, Error, TEXT("LocalizationSubsystem: no DataAsset for locale '%s' — LocalizationData%s is NULL. Check BP_MyGameInstance defaults."),
            *InLocale, bIsRussian ? TEXT("RU") : TEXT("EN"));
        return;
    }

    CurrentLocale = InLocale;
    SetLocalizationData(Target);

    FInternationalization::Get().SetCurrentCulture(InLocale);

    // Persist the choice
    GConfig->SetString(TEXT("Localization"), TEXT("Locale"), *InLocale, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);

    OnLocaleChanged.Broadcast(CurrentLocale);

    UE_LOG(LogGameplay, Verbose, TEXT("LocalizationSubsystem: locale set to '%s'"), *InLocale);
}

// ?? Private helpers ???????????????????????????????????????????????????????????

