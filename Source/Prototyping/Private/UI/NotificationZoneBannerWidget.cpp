#include "UI/NotificationZoneBannerWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "TimerManager.h"

void UNotificationZoneBannerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UNotificationZoneBannerWidget::ShowZoneBanner(const FWorldNotificationStruct& Notification)
{
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(AutoHideTimer);

    const FString ZoneSlug = Notification.dataFields.FindRef(TEXT("zoneSlug"));
    const FString MinLvl   = Notification.dataFields.FindRef(TEXT("minLevel"));
    const FString MaxLvl   = Notification.dataFields.FindRef(TEXT("maxLevel"));
    const FString IsPvp    = Notification.dataFields.FindRef(TEXT("isPvp"));
    const FString IsSafe   = Notification.dataFields.FindRef(TEXT("isSafeZone"));

    ULocalizationSubsystem* Loc = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        Loc = GI->GetSubsystem<ULocalizationSubsystem>();

    FText ZoneName = Loc ? Loc->GetZoneDisplayName(ZoneSlug) : FText::FromString(ZoneSlug);
    if (Zone_Name_Text)
        Zone_Name_Text->SetText(ZoneName);

    if (Zone_Sub_Text)
    {
        FString Sub;
        if (!MinLvl.IsEmpty() && !MaxLvl.IsEmpty())
        {
            // Use locale label if available, otherwise English fallback
            FText LevelLabel = Loc
                ? Loc->GetNotificationTextTemplate(TEXT("zone_level_range"))
                : FText::GetEmpty();
            Sub = LevelLabel.IsEmpty()
                ? FString::Printf(TEXT("Level %s \u2014 %s"), *MinLvl, *MaxLvl)
                : FString::Printf(TEXT("%s %s \u2014 %s"), *LevelLabel.ToString(), *MinLvl, *MaxLvl);
        }
        FText PvpLabel  = Loc ? Loc->GetNotificationTextTemplate(TEXT("zone_flag_pvp"))      : FText::GetEmpty();
        FText SafeLabel = Loc ? Loc->GetNotificationTextTemplate(TEXT("zone_flag_safe"))     : FText::GetEmpty();
        if (IsPvp == TEXT("true"))
            Sub += TEXT("  ") + (PvpLabel.IsEmpty()  ? FString(TEXT("[PvP]"))       : PvpLabel.ToString());
        if (IsSafe == TEXT("true"))
            Sub += TEXT("  ") + (SafeLabel.IsEmpty() ? FString(TEXT("[Safe Zone]")) : SafeLabel.ToString());
        Zone_Sub_Text->SetText(FText::FromString(Sub));
    }

    if (Zone_PvP_Icon)
        Zone_PvP_Icon->SetVisibility(IsPvp == TEXT("true") ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Zone_Safe_Icon)
        Zone_Safe_Icon->SetVisibility(IsSafe == TEXT("true") ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    SetVisibility(ESlateVisibility::HitTestInvisible);
    PlayShowAnimation();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(AutoHideTimer, FTimerDelegate::CreateWeakLambda(this,
            [this]() { PlayHideAnimation(); }), DisplayDuration, false);
    }
}
