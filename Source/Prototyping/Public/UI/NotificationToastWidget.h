#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "NotificationToastWidget.generated.h"

class UTextBlock;
class UImage;

/**
 * NotificationToastWidget
 *
 * Small pop-up notification for channel "toast".
 * Appears at the top-right corner (or wherever anchored in UMG),
 * auto-dismisses after DisplayDuration seconds.
 *
 * Handles notification types:
 *   zone_explored, bestiary_tier_unlocked, zone_entered (secondary toast),
 *   mastery_tier_up, and any unknown type as fallback.
 *
 * Blueprint subclass must bind:
 *   Toast_Title_Text   UTextBlock   — bold title line  (BindWidgetOptional)
 *   Toast_Body_Text    UTextBlock   — body / detail line
 *   Toast_Icon         UImage       (BindWidgetOptional)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UNotificationToastWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Show the toast for the given notification.
     * Automatically hides after DisplayDuration (default 4 s).
     */
    UFUNCTION(BlueprintCallable, Category = "Notification|Toast")
    void ShowNotification(const FWorldNotificationStruct& Notification);

    /** How many seconds the toast stays visible before auto-hiding. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Toast|Config")
    float DisplayDuration = 4.0f;

protected:
    virtual void NativeConstruct() override;

    /** Override in Blueprint for custom show/hide animation. */
    UFUNCTION(BlueprintNativeEvent, Category = "Notification|Toast")
    void PlayShowAnimation();
    virtual void PlayShowAnimation_Implementation() {}

    UFUNCTION(BlueprintNativeEvent, Category = "Notification|Toast")
    void PlayHideAnimation();
    virtual void PlayHideAnimation_Implementation() { SetVisibility(ESlateVisibility::Collapsed); }

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Notification|Toast")
    UTextBlock* Toast_Title_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Notification|Toast")
    UTextBlock* Toast_Body_Text = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Notification|Toast")
    UImage* Toast_Icon = nullptr;

private:
    /** Build the display text from the notification data + locale. */
    void BuildDisplayText(const FWorldNotificationStruct& Notification,
                          FText& OutTitle, FText& OutBody) const;

    FTimerHandle AutoHideTimer;
};
