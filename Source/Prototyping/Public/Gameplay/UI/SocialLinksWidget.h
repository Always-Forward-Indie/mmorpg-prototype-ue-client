#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "SocialLinksWidget.generated.h"

UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API USocialLinksWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Social Links")
	FString TelegramUrl;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Social Links")
	FString WebsiteUrl;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Social Links")
	FString TwitterUrl;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Social Links")
	FString YoutubeUrl;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Social Links")
	FString DiscordUrl;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Btn_Telegram;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Btn_Website;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Btn_Twitter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Btn_Youtube;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* Btn_Discord;

private:
	static void LaunchURL(const FString& URL);

	UFUNCTION() void HandleTelegramClicked();
	UFUNCTION() void HandleWebsiteClicked();
	UFUNCTION() void HandleTwitterClicked();
	UFUNCTION() void HandleYoutubeClicked();
	UFUNCTION() void HandleDiscordClicked();
};
