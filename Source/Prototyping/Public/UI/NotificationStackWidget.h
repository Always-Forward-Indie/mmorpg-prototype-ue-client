#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "UI/NotificationToastWidget.h"
#include "NotificationStackWidget.generated.h"

class UVerticalBox;

/**
 * UNotificationStackWidget
 *
 * AAA-style stacked toasts: up to MaxVisible entries on screen at once
 * (newest on top), overflow waits in a queue and promotes as entries
 * expire. Each entry is a pooled UNotificationToastWidget so show/hide
 * animations and locale text stay in one place.
 *
 * Severity (green / white / red title + Notification / Error sound) is
 * derived from the notification type: *failed* errors, reward receipts
 * (received/learned/unlocked/up/explored) success, everything else info.
 *
 * Created in C++ by UIManager (no Blueprint subclass needed); entry
 * visuals come from EntryClass (the existing WBP toast). Call
 * AnchorTopRight() after AddToViewport.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UNotificationStackWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Show a notification now, or queue it when all entries are busy. */
    UFUNCTION(BlueprintCallable, Category = "Notification|Stack")
    void ShowNotification(const FWorldNotificationStruct& Notification);

    /** Build a lightweight "action" notification and show it. */
    UFUNCTION(BlueprintCallable, Category = "Notification|Stack")
    void EnqueueActionToast(const FString& NotificationType, const TMap<FString, FString>& DataFields);

    /** Pin the stack to the viewport's top-right corner (call after AddToViewport). */
    UFUNCTION(BlueprintCallable, Category = "Notification|Stack")
    void AnchorTopRight();

    /** Widget class used for stack entries (the WBP toast). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Stack|Config")
    TSubclassOf<UNotificationToastWidget> EntryClass;

    /** Maximum entries on screen at once (newest on top). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Stack|Config")
    int32 MaxVisible = 5;

    /** Maximum queued notifications waiting for a free entry. Oldest dropped. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Stack|Config")
    int32 MaxQueueSize = 8;

    /** Seconds each entry stays visible (applied to spawned entries). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification|Stack|Config")
    float DisplayDuration = 4.0f;

protected:
    virtual void NativeConstruct() override;

private:
    /** Severity derived from the notification type (display + sound only). */
    static EShopStatusSeverity SeverityForType(const FString& NotificationType);

    void PlaySeveritySound(EShopStatusSeverity Severity);

    /** Free hidden entry, or a new one when under MaxVisible; null when full. */
    UNotificationToastWidget* AcquireEntry();

    /** Show the next queued notification if an entry is free. */
    void PromoteNext();

    /** Entry expiry callback — collapse, recycle and promote. */
    void HandleEntryExpired(UNotificationToastWidget* ExpiredEntry);

    /** Newest-first ordering after every show/promote. */
    void ReorderNewestFirst();

    UPROPERTY()
    UVerticalBox* StackBox = nullptr;

    UPROPERTY()
    TArray<UNotificationToastWidget*> Entries;

    /** Show sequence per entry (monotonic) — newest shown sits on top. */
    TMap<UNotificationToastWidget*, int32> ShowSequence;
    int32 ShowCounter = 0;

    TArray<FWorldNotificationStruct> PendingQueue;
};
