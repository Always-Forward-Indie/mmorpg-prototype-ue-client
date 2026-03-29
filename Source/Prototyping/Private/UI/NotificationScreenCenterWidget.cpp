#include "UI/NotificationScreenCenterWidget.h"
#include "Components/TextBlock.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "TimerManager.h"

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
            : FText::FromString(TEXT("LEVEL UP!"));

        FText SubTemplate = (bHasLocale && !Def.textTemplate.IsEmpty())
            ? Def.textTemplate
            : FText::GetEmpty();
        Sub = SubTemplate.IsEmpty()
            ? FText::FromString(FString::Printf(TEXT("You are now level %s"), *NewLevel))
            : FText::FromString(FString::Printf(TEXT("%s %s"), *SubTemplate.ToString(), *NewLevel));
    }
    else
    {
        // Generic fallback
        ULocalizationSubsystem* Loc = nullptr;
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
            Loc = GI->GetSubsystem<ULocalizationSubsystem>();
        Title = Loc ? Loc->GetNotificationTextTemplate(Type) : FText::FromString(Type);
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
