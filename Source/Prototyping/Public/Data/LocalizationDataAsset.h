#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "LocalizationDataAsset.generated.h"

/**
 * LocalizationDataAsset
 *
 * Singleton-like data asset assigned in MyGameInstance (BP defaults).
 * Holds soft references to all DataTable assets that map server clientKeys
 * to localised FText and additional metadata (icons, sounds).
 *
 * Assign in editor:
 *   QuestDefinitions          ? DT_QuestDefinitions          (FQuestDefinition)
 *   QuestStepDefinitions      ? DT_QuestStepDefinitions      (FQuestStepDefinition)
 *   DialogueNodes             ? DT_DialogueNodes             (FDialogueNodeDefinition)
 *   DialogueChoices           ? DT_DialogueChoices           (FDialogueChoiceDefinition)
 *   ItemLocale                ? DT_ItemLocale                (FItemLocaleDefinition)
 *   MobLocale                 ? DT_MobLocale                 (FMobLocaleDefinition)
 *   BestiaryCategories        ? DT_BestiaryCategories        (FBestiaryCategoryDefinition)
 *   ZoneLocale                ? DT_ZoneLocale                (FZoneLocaleDefinition)
 *   NotificationLocale        ? DT_NotificationLocale        (FNotificationLocaleDefinition)
 */
UCLASS(BlueprintType)
class PROTOTYPING_API ULocalizationDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // ?? Quests ???????????????????????????????????????????????????????????????

    // Maps clientQuestKey ? FQuestDefinition
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Quests")
    TObjectPtr<UDataTable> QuestDefinitions;

    // Maps clientStepKey ? FQuestStepDefinition
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Quests")
    TObjectPtr<UDataTable> QuestStepDefinitions;

    // ?? Dialogue ?????????????????????????????????????????????????????????????

    // Maps clientNodeKey ? FDialogueNodeDefinition
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Dialogue")
    TObjectPtr<UDataTable> DialogueNodes;

    // Maps clientChoiceKey ? FDialogueChoiceDefinition
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Dialogue")
    TObjectPtr<UDataTable> DialogueChoices;

    // ?? Items ?????????????????????????????????????????????????????????????????

    // Maps item slug ? FItemLocaleDefinition  (name + description)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Items")
    TObjectPtr<UDataTable> ItemLocale;

    // ?? Mobs ??????????????????????????????????????????????????????????????????

    // Maps mob slug ? FMobLocaleDefinition  (name + description + lore)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Mobs")
    TObjectPtr<UDataTable> MobLocale;

    // ?? Bestiary ??????????????????????????????????????????????????????????????

    // Maps categorySlug ? FBestiaryCategoryDefinition  (tab title + locked hint)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Bestiary")
    TObjectPtr<UDataTable> BestiaryCategories;

    // ?? Zones ?????????????????????????????????????????????????????????????????

    // Maps zone slug ? FZoneLocaleDefinition  (zone name + description)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Zones")
    TObjectPtr<UDataTable> ZoneLocale;

    // ?? NPCs ???????????????????????????????????????????????????????????????????

    // Maps npc slug ? FNPCLocaleDefinition  (name + description)
    // Optional: if not assigned, NPC display names fall back to the raw slug.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|NPCs")
    TObjectPtr<UDataTable> NPCLocale;

    // -- Titles ----------------------------------------------------------------

    // Maps title slug -> FTitleLocaleDefinition  (display name + description)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Titles")
    TObjectPtr<UDataTable> TitleLocale;

    // -- Masteries ---------------------------------------------------------------

    // Maps mastery slug -> FMasteryLocaleDefinition  (display name + description)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Mastery")
    TObjectPtr<UDataTable> MasteryLocale;

    // -- Effects ---------------------------------------------------------------

    // Maps effect slug -> FEffectLocaleDefinition  (display name + description)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Effects")
    TObjectPtr<UDataTable> EffectLocale;

    // -- World Notifications -------------------------------------------------

    // Maps notificationType -> FNotificationLocaleDefinition  (text template + icon + sound)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|Notifications")
    TObjectPtr<UDataTable> NotificationLocale;

    // -- NPC Ambient Speech --------------------------------------------------

    /**
     * Maps an ambient speech lineKey -> FAmbientSpeechLineDefinition
     * (localised speech text + optional sound + display duration override).
     * Row name = lineKey exactly as sent by the server in FAmbientSpeechLineData.lineKey.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|AmbientSpeech")
    TObjectPtr<UDataTable> AmbientSpeechLines;

    // -- World Interactive Objects --------------------------------------------

    /**
     * Maps WIO nameKey -> FWIOLocaleDefinition
     * (localised name + description + interaction prompt).
     * Row name = nameKey exactly as sent by the server (e.g. "wio.ancient_altar").
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization|WorldObjects")
    TObjectPtr<UDataTable> WorldObjectLocale;
};
