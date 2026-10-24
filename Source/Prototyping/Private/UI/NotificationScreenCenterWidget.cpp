#include "UI/NotificationScreenCenterWidget.h"
#include "Components/TextBlock.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "TimerManager.h"

// S5: screen-center chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "ScreenCenter"

void UNotificationScreenCenterWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UNotificationScreenCenterWidget::ShowScreenCenter(const FWorldNotificationStruct& Notification)
{
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(AutoHideTimer);

    FText Title, Sub;
    const FString& Type = Notification.notificationType;

    if (Type == TEXT("level_up"))
    {
        const FString NewLevel = Notification.dataFields.FindRef(TEXT("newLevel"));

        ULocalizationSubsystem* Loc = nullptr;
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
            Loc = GI->GetSubsystem<ULocalizationSubsystem>();

        FNotificationLocaleDefinition Def;
        const bool bHasLocale = Loc && Loc->GetNotificationLocaleDefinition(Type, Def);

        Title = (bHasLocale && !Def.title.IsEmpty())
            ? Def.title
            : LOCTEXT("LevelUp", "LEVEL UP!");

        FText SubTemplate = (bHasLocale && !Def.textTemplate.IsEmpty())
            ? Def.textTemplate
            : FText::GetEmpty();
        Sub = SubTemplate.IsEmpty()
            ? FText::Format(LOCTEXT("LevelUpBody", "You are now level {0}"), FText::FromString(NewLevel))
            : FText::Format(LOCTEXT("LevelUpBodyTpl", "{0} {1}"), SubTemplate, FText::FromString(NewLevel));
    }
    else
    {
        // Generic fallback
        ULocalizationSubsystem* Loc = nullptr;
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
            Loc = GI->GetSubsystem<ULocalizationSubsystem>();
        if (Type == TEXT("champion_spawned") || Type == TEXT("champion_spawned_soon") || Type == TEXT("rare_mob_spawned"))
{
FString MobSlug = Notification.dataFields.FindRef(TEXT("mobSlug"));
if (MobSlug.IsEmpty()) MobSlug = Notification.dataFields.FindRef(TEXT("slug"));
if (UMyGameInstance* ChampionGI = Cast<UMyGameInstance>(GetGameInstance()))
{
if (ULocalizationSubsystem* ChampionLoc = ChampionGI->GetSubsystem<ULocalizationSubsystem>())
{
const FText MobName = ChampionLoc->GetMobDisplayName(MobSlug);
if (!MobName.IsEmpty()) MobSlug = MobName.ToString();
}
}
if (Type == TEXT("rare_mob_spawned"))
{
Title = LOCTEXT("RareMob", "RARE MOB!");
Sub = FText::FromString(MobSlug);
}
else
{
const FString TrophySlug = Notification.dataFields.FindRef(TEXT("trophySlug"));
FString TrophyLine = MobSlug;
if (!TrophySlug.IsEmpty()) TrophyLine += FString::Printf(TEXT(" - Trophy: %s"), *TrophySlug);
Title = (Type == TEXT("champion_spawned_soon"))
    ? LOCTEXT("ChampionApproaching", "CHAMPION APPROACHING!")
    : LOCTEXT("Champion", "CHAMPION!");
Sub = FText::FromString(TrophyLine);
}
Loc = nullptr;
}
else
{
Title = Loc ? Loc->GetNotificationTextTemplate(Type) : FText::FromString(Type);
}
        if (Type != TEXT("champion_spawned") && Type != TEXT("champion_spawned_soon") && Type != TEXT("rare_mob_spawned"))
Sub   = FText::GetEmpty();
    }

    if (Center_Title_Text) Center_Title_Text->SetText(Title);
    if (Center_Sub_Text)   Center_Sub_Text->SetText(Sub);

    SetVisibility(ESlateVisibility::HitTestInvisible);
    PlayShowAnimation();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(AutoHideTimer, FTimerDelegate::CreateWeakLambda(this,
            [this]() { PlayHideAnimation(); }), DisplayDuration, false);
    }
}

#undef LOCTEXT_NAMESPACE
