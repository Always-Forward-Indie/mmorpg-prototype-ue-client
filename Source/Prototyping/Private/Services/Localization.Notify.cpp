#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

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

