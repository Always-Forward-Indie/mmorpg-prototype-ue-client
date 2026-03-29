#include "UI/NotificationAtmosphereWidget.h"
#include "Components/TextBlock.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "TimerManager.h"

void UNotificationAtmosphereWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UNotificationAtmosphereWidget::ShowAtmosphere(const FWorldNotificationStruct& Notification)
{
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(AutoHideTimer);

    FText AtmoText;
    const FString& Type = Notification.notificationType;

    ULocalizationSubsystem* Loc = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        Loc = GI->GetSubsystem<ULocalizationSubsystem>();

    if (Loc)
    {
        // Locale key: e.g. "pity_hint" -> locale template "You have been hunting here for a long time..."
        AtmoText = Loc->GetNotificationTextTemplate(Type);
    }

    if (AtmoText.IsEmpty())
        AtmoText = FText::FromString(Type);

    if (Atmo_Text)
        Atmo_Text->SetText(AtmoText);

    SetVisibility(ESlateVisibility::HitTestInvisible);
    PlayShowAnimation();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(AutoHideTimer, FTimerDelegate::CreateWeakLambda(this,
            [this]() { PlayHideAnimation(); }), DisplayDuration, false);
    }
}
