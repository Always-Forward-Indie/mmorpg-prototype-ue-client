#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "NotificationZoneBannerWidget.generated.h"

class UTextBlock;
class UImage;

/**
 * NotificationZoneBannerWidget
 *
 * Large banner shown when the player enters a new zone (channel "zone_banner").
 * Fades in/out automatically.
 *
 * Blueprint subclass must bind:
 *   Zone_Name_Text    UTextBlock   — localized zone name
 *   Zone_Sub_Text     UTextBlock   — level range + PvP/safe label (BindWidgetOptional)
 *   Zone_PvP_Icon     UImage       (BindWidgetOptional)
 *   Zone_Safe_Icon    UImage       (BindWidgetOptional)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UNotificationZoneBannerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Notification|ZoneBanner")
    void ShowZoneBanner(const FWorldNotificationStruct& Notification);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|ZoneBanner|Config")
    float DisplayDuration = 4.0f;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintNativeEvent, Category = "Notification|ZoneBanner")
    void PlayShowAnimation();
    virtual void PlayShowAnimation_Implementation() {}

    UFUNCTION(BlueprintNativeEvent, Category = "Notification|ZoneBanner")
    void PlayHideAnimation();
    virtual void PlayHideAnimation_Implementation() { SetVisibility(ESlateVisibility::Collapsed); }

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Notification|ZoneBanner")
    UTextBlock* Zone_Name_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Notification|ZoneBanner")
    UTextBlock* Zone_Sub_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Notification|ZoneBanner")
    UImage* Zone_PvP_Icon = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Notification|ZoneBanner")
    UImage* Zone_Safe_Icon = nullptr;

private:
    FTimerHandle AutoHideTimer;
};
