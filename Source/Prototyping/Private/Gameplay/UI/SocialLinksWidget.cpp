#include "Gameplay/UI/SocialLinksWidget.h"
#include "HAL/PlatformProcess.h"

void USocialLinksWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Telegram && !TelegramUrl.IsEmpty())
		Btn_Telegram->OnClicked.AddDynamic(this, &USocialLinksWidget::HandleTelegramClicked);
	else if (Btn_Telegram)
		Btn_Telegram->SetVisibility(ESlateVisibility::Collapsed);

	if (Btn_Website && !WebsiteUrl.IsEmpty())
		Btn_Website->OnClicked.AddDynamic(this, &USocialLinksWidget::HandleWebsiteClicked);
	else if (Btn_Website)
		Btn_Website->SetVisibility(ESlateVisibility::Collapsed);

	if (Btn_Twitter && !TwitterUrl.IsEmpty())
		Btn_Twitter->OnClicked.AddDynamic(this, &USocialLinksWidget::HandleTwitterClicked);
	else if (Btn_Twitter)
		Btn_Twitter->SetVisibility(ESlateVisibility::Collapsed);

	if (Btn_Youtube && !YoutubeUrl.IsEmpty())
		Btn_Youtube->OnClicked.AddDynamic(this, &USocialLinksWidget::HandleYoutubeClicked);
	else if (Btn_Youtube)
		Btn_Youtube->SetVisibility(ESlateVisibility::Collapsed);

	if (Btn_Discord && !DiscordUrl.IsEmpty())
		Btn_Discord->OnClicked.AddDynamic(this, &USocialLinksWidget::HandleDiscordClicked);
	else if (Btn_Discord)
		Btn_Discord->SetVisibility(ESlateVisibility::Collapsed);
}

void USocialLinksWidget::LaunchURL(const FString& URL)
{
	if (!URL.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
	}
}

void USocialLinksWidget::HandleTelegramClicked()  { LaunchURL(TelegramUrl); }
void USocialLinksWidget::HandleWebsiteClicked()   { LaunchURL(WebsiteUrl); }
void USocialLinksWidget::HandleTwitterClicked()   { LaunchURL(TwitterUrl); }
void USocialLinksWidget::HandleYoutubeClicked()   { LaunchURL(YoutubeUrl); }
void USocialLinksWidget::HandleDiscordClicked()   { LaunchURL(DiscordUrl); }
