#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/DataStructs.h"
#include "Data/WIODataStructs.h"
#include "LocalizationSubsystem.generated.h"

class ULocalizationDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLocaleChanged, const FString&, NewLocale);

/**
 * LocalizationSubsystem
 *
 * GameInstance subsystem that resolves server slugs / clientKeys to localised
 * FText (and optional metadata) by looking up the DataTables configured in
 * ULocalizationDataAsset.
 *
 * Supports multiple languages by holding separate DataAssets per locale.
 * The active locale is persisted to GameUserSettings.ini.
 *
 * Usage:
 *   ULocalizationSubsystem* Loc = GetGameInstance()->GetSubsystem<ULocalizationSubsystem>();
 *   Loc->SetLocale(TEXT("en"));
 *   FText Title = Loc->GetQuestDisplayName("quest.wolf_hunt");
 */
UCLASS()
class PROTOTYPING_API ULocalizationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Localization")
    void SetLocalizationData(ULocalizationDataAsset* InAsset);

    UFUNCTION(BlueprintCallable, Category = "Localization")
    void SetLocalizationDataRU(ULocalizationDataAsset* InAsset) { LocalizationDataRU = InAsset; }

    UFUNCTION(BlueprintCallable, Category = "Localization")
    void SetLocalizationDataEN(ULocalizationDataAsset* InAsset) { LocalizationDataEN = InAsset; }

    /** Set the active locale ("en" or "ru"). Persists to config and broadcasts OnLocaleChanged. */
    UFUNCTION(BlueprintCallable, Category = "Localization")
    void SetLocale(const FString& InLocale);

    /** Returns the active locale code, e.g. "en" or "ru". */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization")
    FString GetCurrentLocale() const { return CurrentLocale; }

    /** Broadcast when the locale changes so open widgets can refresh their text. */
    UPROPERTY(BlueprintAssignable, Category = "Localization|Events")
    FOnLocaleChanged OnLocaleChanged;

    // ?? Quests ???????????????????????????????????????????????????????????????

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Quest")
    FText GetQuestDisplayName(const FString& ClientQuestKey) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Quest")
    FText GetQuestDescription(const FString& ClientQuestKey) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Quest")
    bool GetQuestDefinition(const FString& ClientQuestKey, FQuestDefinition& OutDefinition) const;

    // ?? Quest Steps ???????????????????????????????????????????????????????????

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Quest")
    FText GetQuestStepDescription(const FString& ClientStepKey) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Quest")
    FText GetQuestStepHint(const FString& ClientStepKey) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Quest")
    bool GetQuestStepDefinition(const FString& ClientStepKey, FQuestStepDefinition& OutDefinition) const;

    // ?? Dialogue Nodes ????????????????????????????????????????????????????????

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Dialogue")
    FText GetDialogueNodeText(const FString& ClientNodeKey) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Dialogue")
    bool GetDialogueNodeDefinition(const FString& ClientNodeKey, FDialogueNodeDefinition& OutDefinition) const;

    // ?? Dialogue Choices ??????????????????????????????????????????????????????

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Dialogue")
    FText GetDialogueChoiceText(const FString& ClientChoiceKey) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Dialogue")
    bool GetDialogueChoiceDefinition(const FString& ClientChoiceKey, FDialogueChoiceDefinition& OutDefinition) const;

    // ?? Items ?????????????????????????????????????????????????????????????????

    /** Localised item name. Key = item slug (e.g. "iron_sword"). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Items")
    FText GetItemDisplayName(const FString& ItemSlug) const;

    /** Localised item description. Key = item slug. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Items")
    FText GetItemDescription(const FString& ItemSlug) const;

    /** Full row. Returns false if not found. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Items")
    bool GetItemLocaleDefinition(const FString& ItemSlug, FItemLocaleDefinition& OutDefinition) const;

    // ?? Mobs ??????????????????????????????????????????????????????????????????

    /** Localised mob name. Key = mob slug (e.g. "forest_wolf"). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Mobs")
    FText GetMobDisplayName(const FString& MobSlug) const;

    // ?? NPCs ???????????????????????????????????????????????????????????????????

    /** Localised NPC name. Key = npc slug (e.g. "merchant_tom"). Falls back to the raw slug. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|NPCs")
    FText GetNPCDisplayName(const FString& NPCSlug) const;

    /** Localised mob description. Key = mob slug. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Mobs")
    FText GetMobDescription(const FString& MobSlug) const;

    /** Lore text for bestiary tier 2. Key = loreKey from server. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Mobs")
    FText GetMobLoreText(const FString& LoreKey) const;

    /** Full row. Returns false if not found. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Mobs")
    bool GetMobLocaleDefinition(const FString& MobSlug, FMobLocaleDefinition& OutDefinition) const;

    // ?? Bestiary Categories ???????????????????????????????????????????????????

    /** Localised category title. Key = categorySlug (e.g. "combat_info"). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Bestiary")
    FText GetBestiaryCategoryName(const FString& CategorySlug) const;

    /** Locked-tier hint text. Key = categorySlug. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Bestiary")
    FText GetBestiaryCategoryLockedHint(const FString& CategorySlug) const;

    /** Full row. Returns false if not found. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Bestiary")
    bool GetBestiaryCategoryDefinition(const FString& CategorySlug, FBestiaryCategoryDefinition& OutDefinition) const;

    // ?? Zones ?????????????????????????????????????????????????????????????????

    /** Localised zone name. Key = zone slug (e.g. "dead_forest"). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Zones")
    FText GetZoneDisplayName(const FString& ZoneSlug) const;

    /** Localised zone description. Key = zone slug. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Zones")
    FText GetZoneDescription(const FString& ZoneSlug) const;

    /** Full row. Returns false if not found. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Zones")
    bool GetZoneLocaleDefinition(const FString& ZoneSlug, FZoneLocaleDefinition& OutDefinition) const;

    // ?? World Notifications ????????????????????????????????????????????????

    /** Localised text template. Key = notificationType (e.g. "pity_hint"). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Notifications")
    FText GetNotificationTextTemplate(const FString& NotificationType) const;

    /** Full row (template, title, icon, sound). Returns false if not found. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Notifications")
    bool GetNotificationLocaleDefinition(const FString& NotificationType, FNotificationLocaleDefinition& OutDefinition) const;

    // ?? Vendor / Trade Error Codes ?????????????????????????????????????????

    /**
     * Localised vendor error message.
     * Key = errorCode slug (e.g. "INSUFFICIENT_GOLD", "OUT_OF_STOCK").
     * Falls back to the raw errorCode string when not found.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Vendor")
    FText GetVendorErrorText(const FString& ErrorCode) const;

    /**
     * Localised trade cancel reason string.
     * Key = reason slug (e.g. "PARTNER_CANCELLED", "TIMEOUT").
     * Falls back to the raw reason string when not found.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Trade")
    FText GetTradeCancelReasonText(const FString& Reason) const;

    /**
     * Localised "item broken" notification text.
     * Key = item slug; returns a template like "Your {item} has broken!".
     * Falls back to item display name.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Items")
    FText GetItemBrokenText(const FString& ItemSlug) const;

    // -- NPC Ambient Speech --------------------------------------------------

    /**
     * Returns the localised speech text for an ambient speech lineKey.
     * Falls back to FText::FromString(LineKey) when not found.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|AmbientSpeech")
    FText GetNPCSpeechText(const FString& LineKey) const;

    /**
     * Fills OutDefinition with the full FAmbientSpeechLineDefinition for the given lineKey.
     * Returns true on success.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|AmbientSpeech")
    bool GetNPCSpeechLineDefinition(const FString& LineKey, FAmbientSpeechLineDefinition& OutDefinition) const;

    // -- World Interactive Objects -------------------------------------------

    /** Localised WIO name. Key = nameKey from server (e.g. "wio.ancient_altar"). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|WorldObjects")
    FText GetWIODisplayName(const FString& NameKey) const;

    /** Localised WIO description. Key = nameKey. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|WorldObjects")
    FText GetWIODescription(const FString& NameKey) const;

    /** Localised WIO interaction prompt (e.g. "[F] Examine"). Key = nameKey. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|WorldObjects")
    FText GetWIOInteractionPrompt(const FString& NameKey) const;

    /** Full row. Returns false if not found. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|WorldObjects")
    bool GetWIOLocaleDefinition(const FString& NameKey, FWIOLocaleDefinition& OutDefinition) const;

    // -- Titles ----------------------------------------------------------------

    /** Localised title display name. Key = title slug (e.g. "wolf_slayer"). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Titles")
    FText GetTitleDisplayName(const FString& TitleSlug) const;

    /** Localised title description. Key = title slug. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Titles")
    FText GetTitleDescription(const FString& TitleSlug) const;

    /** Full row. Returns false if not found. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Localization|Titles")
    bool GetTitleLocaleDefinition(const FString& TitleSlug, FTitleLocaleDefinition& OutDefinition) const;

private:
    UPROPERTY()
    ULocalizationDataAsset* LocalizationData = nullptr;

    /** Data asset for Russian locale. Assign in the GameInstance Blueprint defaults. */
    UPROPERTY()
    ULocalizationDataAsset* LocalizationDataRU = nullptr;

    /** Data asset for English locale. Assign in the GameInstance Blueprint defaults. */
    UPROPERTY()
    ULocalizationDataAsset* LocalizationDataEN = nullptr;

    /** Active locale code, e.g. "en" or "ru". Defaults to system/UE culture on first run. */
    FString CurrentLocale;

    UPROPERTY() mutable UDataTable* CachedQuestTable              = nullptr;
    UPROPERTY() mutable UDataTable* CachedQuestStepTable          = nullptr;
    UPROPERTY() mutable UDataTable* CachedDialogueNodeTable       = nullptr;
    UPROPERTY() mutable UDataTable* CachedDialogueChoiceTable     = nullptr;
    UPROPERTY() mutable UDataTable* CachedItemLocaleTable         = nullptr;
    UPROPERTY() mutable UDataTable* CachedMobLocaleTable          = nullptr;
    UPROPERTY() mutable UDataTable* CachedNPCLocaleTable          = nullptr;
    UPROPERTY() mutable UDataTable* CachedBestiaryCategoryTable   = nullptr;
    UPROPERTY() mutable UDataTable* CachedZoneLocaleTable         = nullptr;
    UPROPERTY() mutable UDataTable* CachedNotificationLocaleTable = nullptr;
    UPROPERTY() mutable UDataTable* CachedAmbientSpeechTable      = nullptr;
    UPROPERTY() mutable UDataTable* CachedWIOLocaleTable          = nullptr;
    UPROPERTY() mutable UDataTable* CachedTitleLocaleTable        = nullptr;

    UDataTable* GetQuestTable()              const;
    UDataTable* GetQuestStepTable()          const;
    UDataTable* GetDialogueNodeTable()       const;
    UDataTable* GetDialogueChoiceTable()     const;
    UDataTable* GetItemLocaleTable()         const;
    UDataTable* GetMobLocaleTable()          const;
    UDataTable* GetNPCLocaleTable()          const;
    UDataTable* GetBestiaryCategoryTable()   const;
    UDataTable* GetZoneLocaleTable()         const;
    UDataTable* GetNotificationLocaleTable() const;
    UDataTable* GetAmbientSpeechTable()      const;
    UDataTable* GetWIOLocaleTable()          const;
    UDataTable* GetTitleLocaleTable()        const;

    static FText FallbackText(const FString& Key);
};
