#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "Utils/NetCore/ShopStatusStyle.h"
#include "NotificationToastWidget.generated.h"

class UTextBlock;
class UImage;
class UNotificationToastWidget;

/** Fired when an entry's auto-hide timer elapses (after its hide starts). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnToastExpired, UNotificationToastWidget* /*ExpiredEntry*/);

/**
 * NotificationToastWidget
 *
 * Single toast entry renderer. Shows ONE notification, auto-hides after
 * DisplayDuration, then reports expiry via OnToastExpired so the owning
 * UNotificationStackWidget can recycle the entry and promote the queue.
 *
 * Appears at the top-right corner (or wherever the stack anchors it),
 * severity-tinted via SetSeverity (green receipt / white info / red error).
 *
 * Handles notification types:
 *   zone_explored, bestiary_tier_unlocked, zone_entered (secondary toast),
 *   mastery_tier_up, and any unknown type as fallback.
 *
 * Blueprint subclass must bind:
 *   Toast_Title_Text   UTextBlock — bold title line  (BindWidgetOptional)
 *   Toast_Body_Text    UTextBlock — body / detail line
 *   Toast_Icon         UImage       (BindWidgetOptional)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UNotificationToastWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Fired when the auto-hide timer elapses (after PlayHideAnimation starts). */
    FOnToastExpired OnToastExpired;

    /**
     * Show a single notification immediately, replacing any current content.
     * The entry hides itself after DisplayDuration and broadcasts OnToastExpired.
     */
    UFUNCTION(BlueprintCallable, Category = "Notification|Toast")
    void ShowNotification(const FWorldNotificationStruct& Notification);

    /**
     * Build a lightweight "action" notification and show it immediately.
     */
    UFUNCTION(BlueprintCallable, Category = "Notification|Toast")
    void EnqueueActionToast(const FString& NotificationType, const TMap<FString, FString>& DataFields);

    /** Severity tint applied to the title line (default Info). */
    UFUNCTION(BlueprintCallable, Category = "Notification|Toast")
    void SetSeverity(EShopStatusSeverity InSeverity);

    /** True while this entry is on screen (between show and hide). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Notification|Toast")
    bool IsShowing() const { return bIsShowing; }

    /** Current body text (empty until shown). Test hook for the toast stack. */
    FText GetCurrentBody() const;

    /** Hide immediately and report expiry (used when the stack recycles us). */
    UFUNCTION(BlueprintCallable, Category = "Notification|Toast")
    void Dismiss();

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

    /** Display the notification immediately (no queue check). */
    void DisplayNotificationImmediate(const FWorldNotificationStruct& Notification);

    void OnAutoHideElapsed();

    FTimerHandle AutoHideTimer;

    /** True while a toast is visible (between show and hide). */
    bool bIsShowing = false;

    /** Latched on first display: NativeConstruct refires when the stack
     *  re-parents the entry, and must not wipe live state then. */
    bool bEverShown = false;

    EShopStatusSeverity Severity = EShopStatusSeverity::Info;
};
