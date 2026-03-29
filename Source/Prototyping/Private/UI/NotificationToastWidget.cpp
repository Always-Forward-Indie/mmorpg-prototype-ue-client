#include "UI/NotificationToastWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "TimerManager.h"

void UNotificationToastWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UNotificationToastWidget::ShowNotification(const FWorldNotificationStruct& Notification)
{
    // Clear previous auto-hide timer
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(AutoHideTimer);

    FText Title, Body;
    BuildDisplayText(Notification, Title, Body);

    if (Toast_Title_Text)
        Toast_Title_Text->SetText(Title);
    if (Toast_Body_Text)
        Toast_Body_Text->SetText(Body.IsEmpty() ? Title : Body);

    // Optional icon from locale
    if (Toast_Icon)
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                FNotificationLocaleDefinition Def;
                if (Loc->GetNotificationLocaleDefinition(Notification.notificationType, Def) && Def.Icon.IsValid())
                {
                    if (UTexture2D* Tex = Def.Icon.LoadSynchronous())
                        Toast_Icon->SetBrushFromTexture(Tex);
                }
            }
        }
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);
    PlayShowAnimation();

    // Schedule auto-hide
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(AutoHideTimer, FTimerDelegate::CreateWeakLambda(this,
            [this]()
            {
                PlayHideAnimation();
            }), DisplayDuration, false);
    }
}

void UNotificationToastWidget::BuildDisplayText(const FWorldNotificationStruct& Notification,
                                                 FText& OutTitle, FText& OutBody) const
{
    ULocalizationSubsystem* Loc = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        Loc = GI->GetSubsystem<ULocalizationSubsystem>();

    const FString& Type = Notification.notificationType;

    // Try to get the title from the notification locale table first.
    // TextTemplate / Title are filled by designers in DT_NotificationLocale.
    FNotificationLocaleDefinition NotifDef;
    const bool bHasLocale = Loc && Loc->GetNotificationLocaleDefinition(Type, NotifDef);

    if (Type == TEXT("bestiary_tier_unlocked"))
    {
        const FString MobSlug = Notification.dataFields.FindRef(TEXT("mobSlug"));
        const FString CatSlug = Notification.dataFields.FindRef(TEXT("categorySlug"));
        FText MobName = Loc ? Loc->GetMobDisplayName(MobSlug) : FText::FromString(MobSlug);
        FText CatName = Loc ? Loc->GetBestiaryCategoryName(CatSlug) : FText::FromString(CatSlug);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Bestiary unlocked"));
        OutBody  = FText::FromString(FString::Printf(TEXT("%s \u2014 %s"),
            *MobName.ToString(), *CatName.ToString()));
    }
    else if (Type == TEXT("zone_explored"))
    {
        const FString ZoneSlug = Notification.dataFields.FindRef(TEXT("zoneSlug"));
        const FString XpStr    = Notification.dataFields.FindRef(TEXT("xpGained"));
        FText ZoneName = Loc ? Loc->GetZoneDisplayName(ZoneSlug) : FText::FromString(ZoneSlug);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Zone discovered"));
        OutBody  = FText::FromString(FString::Printf(TEXT("%s (+%s XP)"),
            *ZoneName.ToString(), *XpStr));
    }
    else if (Type == TEXT("mastery_tier_up"))
    {
        const FString MasterySlug = Notification.dataFields.FindRef(TEXT("masterySlug"));
        const FString TierStr     = Notification.dataFields.FindRef(TEXT("tier"));
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Mastery increased"));
        OutBody  = FText::FromString(FString::Printf(TEXT("%s \u2014 tier %s"), *MasterySlug, *TierStr));
    }
    else if (Type == TEXT("champion_spawned"))
    {
        const FString MobSlug = Notification.dataFields.FindRef(TEXT("mobSlug"));
        FText MobName = Loc ? Loc->GetMobDisplayName(MobSlug) : FText::FromString(MobSlug);
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Champion appeared!"));
        OutBody  = MobName;
    }
    else if (Type == TEXT("champion_killed"))
    {
        OutTitle = (bHasLocale && !NotifDef.title.IsEmpty()) ? NotifDef.title : FText::FromString(TEXT("Champion defeated!"));
        OutBody  = FText::GetEmpty();
    }
    else
    {
        // Generic fallback: use textTemplate from locale, or the raw type slug
        OutTitle = bHasLocale && !NotifDef.textTemplate.IsEmpty()
            ? NotifDef.textTemplate
            : (Loc ? Loc->GetNotificationTextTemplate(Type) : FText::FromString(Type));
        OutBody = FText::GetEmpty();
    }
}
