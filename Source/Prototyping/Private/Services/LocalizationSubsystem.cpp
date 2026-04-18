#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"

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

    UE_LOG(LogTemp, Log, TEXT("LocalizationSubsystem: data asset %s"),
        InAsset ? TEXT("assigned") : TEXT("cleared"));
}

// ?? Private helpers ???????????????????????????????????????????????????????????

UDataTable* ULocalizationSubsystem::GetQuestTable() const
{
    if (!CachedQuestTable && LocalizationData)
        CachedQuestTable = LocalizationData->QuestDefinitions.LoadSynchronous();
    return CachedQuestTable;
}

UDataTable* ULocalizationSubsystem::GetQuestStepTable() const
{
    if (!CachedQuestStepTable && LocalizationData)
        CachedQuestStepTable = LocalizationData->QuestStepDefinitions.LoadSynchronous();
    return CachedQuestStepTable;
}

UDataTable* ULocalizationSubsystem::GetDialogueNodeTable() const
{
    if (!CachedDialogueNodeTable && LocalizationData)
        CachedDialogueNodeTable = LocalizationData->DialogueNodes.LoadSynchronous();
    return CachedDialogueNodeTable;
}

UDataTable* ULocalizationSubsystem::GetDialogueChoiceTable() const
{
    if (!CachedDialogueChoiceTable && LocalizationData)
        CachedDialogueChoiceTable = LocalizationData->DialogueChoices.LoadSynchronous();
    return CachedDialogueChoiceTable;
}

UDataTable* ULocalizationSubsystem::GetItemLocaleTable() const
{
    if (!CachedItemLocaleTable && LocalizationData)
        CachedItemLocaleTable = LocalizationData->ItemLocale.LoadSynchronous();
    return CachedItemLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetMobLocaleTable() const
{
    if (!CachedMobLocaleTable && LocalizationData)
        CachedMobLocaleTable = LocalizationData->MobLocale.LoadSynchronous();
    return CachedMobLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetNPCLocaleTable() const
{
    if (!CachedNPCLocaleTable && LocalizationData)
        CachedNPCLocaleTable = LocalizationData->NPCLocale.LoadSynchronous();
    return CachedNPCLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetBestiaryCategoryTable() const
{
    if (!CachedBestiaryCategoryTable && LocalizationData)
        CachedBestiaryCategoryTable = LocalizationData->BestiaryCategories.LoadSynchronous();
    return CachedBestiaryCategoryTable;
}

UDataTable* ULocalizationSubsystem::GetZoneLocaleTable() const
{
    if (!CachedZoneLocaleTable && LocalizationData)
        CachedZoneLocaleTable = LocalizationData->ZoneLocale.LoadSynchronous();
    return CachedZoneLocaleTable;
}

UDataTable* ULocalizationSubsystem::GetNotificationLocaleTable() const
{
    if (!CachedNotificationLocaleTable && LocalizationData)
        CachedNotificationLocaleTable = LocalizationData->NotificationLocale.LoadSynchronous();
    return CachedNotificationLocaleTable;
}

FText ULocalizationSubsystem::FallbackText(const FString& Key)
{
    return Key.IsEmpty() ? FText::GetEmpty() : FText::FromString(Key);
}

// ?? Quests ????????????????????????????????????????????????????????????????????

FText ULocalizationSubsystem::GetQuestDisplayName(const FString& ClientQuestKey) const
{
    if (ClientQuestKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetQuestTable();
    if (!Table) return FallbackText(ClientQuestKey);
    const FQuestDefinition* Row = Table->FindRow<FQuestDefinition>(FName(*ClientQuestKey), TEXT("GetQuestDisplayName"), false);
    return (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(ClientQuestKey);
}

FText ULocalizationSubsystem::GetQuestDescription(const FString& ClientQuestKey) const
{
    if (ClientQuestKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetQuestTable();
    if (!Table) return FText::GetEmpty();
    const FQuestDefinition* Row = Table->FindRow<FQuestDefinition>(FName(*ClientQuestKey), TEXT("GetQuestDescription"), false);
    return Row ? Row->description : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetQuestDefinition(const FString& ClientQuestKey, FQuestDefinition& OutDefinition) const
{
    if (ClientQuestKey.IsEmpty()) return false;
    UDataTable* Table = GetQuestTable();
    if (!Table) return false;
    const FQuestDefinition* Row = Table->FindRow<FQuestDefinition>(FName(*ClientQuestKey), TEXT("GetQuestDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Quest Steps ???????????????????????????????????????????????????????????????

FText ULocalizationSubsystem::GetQuestStepDescription(const FString& ClientStepKey) const
{
    if (ClientStepKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetQuestStepTable();
    if (!Table) return FallbackText(ClientStepKey);
    const FQuestStepDefinition* Row = Table->FindRow<FQuestStepDefinition>(FName(*ClientStepKey), TEXT("GetQuestStepDescription"), false);
    return (Row && !Row->description.IsEmpty()) ? Row->description : FallbackText(ClientStepKey);
}

FText ULocalizationSubsystem::GetQuestStepHint(const FString& ClientStepKey) const
{
    if (ClientStepKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetQuestStepTable();
    if (!Table) return FText::GetEmpty();
    const FQuestStepDefinition* Row = Table->FindRow<FQuestStepDefinition>(FName(*ClientStepKey), TEXT("GetQuestStepHint"), false);
    return Row ? Row->hint : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetQuestStepDefinition(const FString& ClientStepKey, FQuestStepDefinition& OutDefinition) const
{
    if (ClientStepKey.IsEmpty()) return false;
    UDataTable* Table = GetQuestStepTable();
    if (!Table) return false;
    const FQuestStepDefinition* Row = Table->FindRow<FQuestStepDefinition>(FName(*ClientStepKey), TEXT("GetQuestStepDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Dialogue Nodes ????????????????????????????????????????????????????????????

FText ULocalizationSubsystem::GetDialogueNodeText(const FString& ClientNodeKey) const
{
    if (ClientNodeKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetDialogueNodeTable();
    if (!Table) return FallbackText(ClientNodeKey);
    const FDialogueNodeDefinition* Row = Table->FindRow<FDialogueNodeDefinition>(FName(*ClientNodeKey), TEXT("GetDialogueNodeText"), false);
    return (Row && !Row->nodeText.IsEmpty()) ? Row->nodeText : FallbackText(ClientNodeKey);
}

bool ULocalizationSubsystem::GetDialogueNodeDefinition(const FString& ClientNodeKey, FDialogueNodeDefinition& OutDefinition) const
{
    if (ClientNodeKey.IsEmpty()) return false;
    UDataTable* Table = GetDialogueNodeTable();
    if (!Table) return false;
    const FDialogueNodeDefinition* Row = Table->FindRow<FDialogueNodeDefinition>(FName(*ClientNodeKey), TEXT("GetDialogueNodeDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Dialogue Choices ??????????????????????????????????????????????????????????

FText ULocalizationSubsystem::GetDialogueChoiceText(const FString& ClientChoiceKey) const
{
    if (ClientChoiceKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetDialogueChoiceTable();
    if (!Table) return FallbackText(ClientChoiceKey);
    const FDialogueChoiceDefinition* Row = Table->FindRow<FDialogueChoiceDefinition>(FName(*ClientChoiceKey), TEXT("GetDialogueChoiceText"), false);
    return (Row && !Row->choiceText.IsEmpty()) ? Row->choiceText : FallbackText(ClientChoiceKey);
}

bool ULocalizationSubsystem::GetDialogueChoiceDefinition(const FString& ClientChoiceKey, FDialogueChoiceDefinition& OutDefinition) const
{
    if (ClientChoiceKey.IsEmpty()) return false;
    UDataTable* Table = GetDialogueChoiceTable();
    if (!Table) return false;
    const FDialogueChoiceDefinition* Row = Table->FindRow<FDialogueChoiceDefinition>(FName(*ClientChoiceKey), TEXT("GetDialogueChoiceDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Items ?????????????????????????????????????????????????????????????????????

FText ULocalizationSubsystem::GetItemDisplayName(const FString& ItemSlug) const
{
    if (ItemSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetItemLocaleTable();
    if (!Table) return FallbackText(ItemSlug);
    const FItemLocaleDefinition* Row = Table->FindRow<FItemLocaleDefinition>(FName(*ItemSlug), TEXT("GetItemDisplayName"), false);
    return (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(ItemSlug);
}

FText ULocalizationSubsystem::GetItemDescription(const FString& ItemSlug) const
{
    if (ItemSlug.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetItemLocaleTable();
    if (!Table) return FText::GetEmpty();
    const FItemLocaleDefinition* Row = Table->FindRow<FItemLocaleDefinition>(FName(*ItemSlug), TEXT("GetItemDescription"), false);
    return Row ? Row->description : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetItemLocaleDefinition(const FString& ItemSlug, FItemLocaleDefinition& OutDefinition) const
{
    if (ItemSlug.IsEmpty()) return false;
    UDataTable* Table = GetItemLocaleTable();
    if (!Table) return false;
    const FItemLocaleDefinition* Row = Table->FindRow<FItemLocaleDefinition>(FName(*ItemSlug), TEXT("GetItemLocaleDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? NPCs ??????????????????????????????????????????????????????????????????????

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

FText ULocalizationSubsystem::GetNotificationTextTemplate(const FString& NotificationType) const
{
    if (NotificationType.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetNotificationLocaleTable();
    if (!Table) return FallbackText(NotificationType);
    const FNotificationLocaleDefinition* Row = Table->FindRow<FNotificationLocaleDefinition>(FName(*NotificationType), TEXT("GetNotificationTextTemplate"), false);
    return (Row && !Row->textTemplate.IsEmpty()) ? Row->textTemplate : FallbackText(NotificationType);
}

bool ULocalizationSubsystem::GetNotificationLocaleDefinition(const FString& NotificationType, FNotificationLocaleDefinition& OutDefinition) const
{
    if (NotificationType.IsEmpty()) return false;
    UDataTable* Table = GetNotificationLocaleTable();
    if (!Table) return false;
    const FNotificationLocaleDefinition* Row = Table->FindRow<FNotificationLocaleDefinition>(FName(*NotificationType), TEXT("GetNotificationLocaleDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Vendor / Trade Error Codes ?????????????????????????????????????????????

FText ULocalizationSubsystem::GetVendorErrorText(const FString& ErrorCode) const
{
    if (ErrorCode.IsEmpty()) return FText::GetEmpty();
    // Vendor error messages use the notification locale table under the key
    // "vendor_error.<errorCode>" (e.g. "vendor_error.INSUFFICIENT_GOLD").
    UDataTable* Table = GetNotificationLocaleTable();
    if (Table)
    {
        const FString Key = FString::Printf(TEXT("vendor_error.%s"), *ErrorCode);
        const FNotificationLocaleDefinition* Row = Table->FindRow<FNotificationLocaleDefinition>(
            FName(*Key), TEXT("GetVendorErrorText"), false);
        if (Row && !Row->textTemplate.IsEmpty()) return Row->textTemplate;
    }
    // Friendly fallback: replace underscores with spaces and title-case
    FString Friendly = ErrorCode.Replace(TEXT("_"), TEXT(" "));
    if (!Friendly.IsEmpty())
        Friendly = Friendly.Left(1).ToUpper() + Friendly.Mid(1).ToLower();
    return FText::FromString(Friendly);
}

FText ULocalizationSubsystem::GetTradeCancelReasonText(const FString& Reason) const
{
    if (Reason.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetNotificationLocaleTable();
    if (Table)
    {
        const FString Key = FString::Printf(TEXT("trade_cancel.%s"), *Reason);
        const FNotificationLocaleDefinition* Row = Table->FindRow<FNotificationLocaleDefinition>(
            FName(*Key), TEXT("GetTradeCancelReasonText"), false);
        if (Row && !Row->textTemplate.IsEmpty()) return Row->textTemplate;
    }
    FString Friendly = Reason.Replace(TEXT("_"), TEXT(" "));
    if (!Friendly.IsEmpty())
        Friendly = Friendly.Left(1).ToUpper() + Friendly.Mid(1).ToLower();
    return FText::FromString(Friendly);
}

FText ULocalizationSubsystem::GetItemBrokenText(const FString& ItemSlug) const
{
    if (ItemSlug.IsEmpty()) return FText::GetEmpty();
    // Try to find a template under "item_broken.<slug>" or generic "item_broken"
    UDataTable* Table = GetNotificationLocaleTable();
    if (Table)
    {
        const FString Key = FString::Printf(TEXT("item_broken.%s"), *ItemSlug);
        const FNotificationLocaleDefinition* Row = Table->FindRow<FNotificationLocaleDefinition>(
            FName(*Key), TEXT("GetItemBrokenText"), false);
        if (Row && !Row->textTemplate.IsEmpty()) return Row->textTemplate;

        const FNotificationLocaleDefinition* Generic = Table->FindRow<FNotificationLocaleDefinition>(
            FName(TEXT("item_broken")), TEXT("GetItemBrokenText"), false);
        if (Generic && !Generic->textTemplate.IsEmpty()) return Generic->textTemplate;
    }
    // Fallback: use the localised item display name
    const FText ItemName = GetItemDisplayName(ItemSlug);
    return FText::FromString(FString::Printf(TEXT("Your %s has broken!"), *ItemName.ToString()));
}



// -- NPC Ambient Speech -----------------------------------------------------

UDataTable* ULocalizationSubsystem::GetAmbientSpeechTable() const
{
    if (!CachedAmbientSpeechTable && LocalizationData)
        CachedAmbientSpeechTable = LocalizationData->AmbientSpeechLines.LoadSynchronous();
    return CachedAmbientSpeechTable;
}

FText ULocalizationSubsystem::GetNPCSpeechText(const FString& LineKey) const
{
    if (LineKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetAmbientSpeechTable();
    if (!Table) return FallbackText(LineKey);
    const FAmbientSpeechLineDefinition* Row = Table->FindRow<FAmbientSpeechLineDefinition>(
        FName(*LineKey), TEXT("GetNPCSpeechText"), false);
    return (Row && !Row->speechText.IsEmpty()) ? Row->speechText : FallbackText(LineKey);
}

bool ULocalizationSubsystem::GetNPCSpeechLineDefinition(const FString& LineKey, FAmbientSpeechLineDefinition& OutDefinition) const
{
    if (LineKey.IsEmpty()) return false;
    UDataTable* Table = GetAmbientSpeechTable();
    if (!Table) return false;
    const FAmbientSpeechLineDefinition* Row = Table->FindRow<FAmbientSpeechLineDefinition>(
        FName(*LineKey), TEXT("GetNPCSpeechLineDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// -- World Interactive Objects -----------------------------------------------

UDataTable* ULocalizationSubsystem::GetWIOLocaleTable() const
{
    if (!CachedWIOLocaleTable && LocalizationData)
        CachedWIOLocaleTable = LocalizationData->WorldObjectLocale.LoadSynchronous();
    return CachedWIOLocaleTable;
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
