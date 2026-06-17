#include "UI/IdleWarningWidget.h"
#include "Components/TextBlock.h"

void UIdleWarningWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UIdleWarningWidget::ShowIdleWarning(int32 TotalSecondsRemaining)
{
	if (CountdownText)
	{
		UpdateCountdown(TotalSecondsRemaining);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	PlayShowAnimation();
}

void UIdleWarningWidget::HideIdleWarning()
{
	PlayHideAnimation();
}

void UIdleWarningWidget::UpdateCountdown(int32 SecondsRemaining)
{
	if (CountdownText)
	{
		CountdownText->SetText(FText::AsNumber(SecondsRemaining));
	}
}
