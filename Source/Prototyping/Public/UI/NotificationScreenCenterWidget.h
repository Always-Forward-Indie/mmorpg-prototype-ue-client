#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "NotificationScreenCenterWidget.generated.h"

class UTextBlock;

/**
 * NotificationScreenCenterWidget
 *
 * Full-screen flash for high-priority notifications (channel "screen_center").
 * Handles: level_up (and future critical announcements).
 *
 * Blueprint subclass must bind:
 *   Center_Title_Text  UTextBlock   — main announcement text
 *   Center_Sub_Text    UTextBlock   (BindWidgetOptional) — sub-line
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UNotificationScreenCenterWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Notification|ScreenCenter")
    void ShowScreenCenter(const FWorldNotificationStruct& Notification);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|ScreenCenter|Config")
    float DisplayDuration = 3.0f;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintNativeEvent, Category = "Notification|ScreenCenter")
    void PlayShowAnimation();
    virtual void PlayShowAnimation_Implementation() {}

    UFUNCTION(BlueprintNativeEvent, Category = "Notification|ScreenCenter")
    void PlayHideAnimation();
    virtual void PlayHideAnimation_Implementation() { SetVisibility(ESlateVisibility::Collapsed); }

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Notification|ScreenCenter")
    UTextBlock* Center_Title_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Notification|ScreenCenter")
    UTextBlock* Center_Sub_Text = nullptr;

private:
    FTimerHandle AutoHideTimer;
};
