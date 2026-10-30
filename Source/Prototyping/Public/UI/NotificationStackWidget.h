#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "UI/NotificationToastWidget.h"
#include "Utils/NetCore/ToastCoalesceRule.h"
#include "NotificationStackWidget.generated.h"

class UCanvasPanel;
class UVerticalBox;

/**
 * UNotificationStackWidget
 *
 * AAA-style stacked toasts: up to MaxVisible entries on screen at once
 * (newest on top), overflow waits in a queue and promotes as entries
 * expire. Each entry is a pooled UNotificationToastWidget so show/hide
 * animations and locale text stay in one place.
 *
 * Layout is code-owned (no Blueprint needed): a fullscreen CanvasPanel root
 * with a VerticalBox child anchored top-right. Entry visuals come from
 * EntryClass (the existing WBP toast).
 *
 * Severity (green / white / red title + Notification / Error sound) is
 * derived from the notification type: *failed* errors, reward receipts
 * (received/learned/unlocked/up/explored) success, everything else info.
 *
 * Coalescing (FToastCoalesceRule): identical gains inside 1.5s merge into
 * one entry (single sound), in the visible pool and in the overflow queue.
 * Losses never toast — gains-only policy, see FToastCoalesceRule::IsGainType.
 *
 * Created in C++ by UIManager; call AnchorTopRight() after AddToViewport
 * (harmless no-op — the canvas slot is pinned at build time).
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

    /** Re-pin the stack box to the viewport's top-right corner. */
    UFUNCTION(BlueprintCallable, Category = "Notification|Stack")
    void AnchorTopRight();

    /** Snapshot for the toast self-test (devmode scenario): pool sizes and
     *  whether the Slate tree was built before first paint. */
    void GetToastDiagnostics(int32& OutTotalEntries, int32& OutVisibleEntries,
        int32& OutQueued, bool& OutTreeBuilt) const;

    /** Bodies of currently visible entries (coalesce self-test hook). */
    void GetVisibleToastBodies(TArray<FString>& OutBodies) const;

    /** How many incoming toasts merged into an existing entry (self-test). */
    int32 GetCoalescedCount() const { return CoalescedCount; }

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
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    /** Severity derived from the notification type (display + sound only). */
    static EShopStatusSeverity SeverityForType(const FString& NotificationType);

    void PlaySeveritySound(EShopStatusSeverity Severity);

    /** Build the canvas root + anchored stack box (idempotent, any order). */
    void EnsureStackBox();

    /** Owning player from the CURRENT world (never a stale cached one). */
    APlayerController* ResolvePlayerController() const;

    /** Free hidden entry, or a new one when under MaxVisible; null when full. */
    UNotificationToastWidget* AcquireEntry();

    /** Show the next queued notification if an entry is free. */
    void PromoteNext();

    /** Entry expiry callback — collapse, recycle and promote. */
    void HandleEntryExpired(UNotificationToastWidget* ExpiredEntry);

    /** Newest-first ordering after every show/promote. */
    void ReorderNewestFirst();

    /** Merge into a matching visible/queued toast inside the window (true). */
    bool TryCoalesce(const FWorldNotificationStruct& Notification);

    UPROPERTY()
    UCanvasPanel* RootCanvas = nullptr;

    UPROPERTY()
    UVerticalBox* StackBox = nullptr;

    UPROPERTY()
    TArray<UNotificationToastWidget*> Entries;

    /** Show sequence per entry (monotonic) — newest shown sits on top. */
    TMap<UNotificationToastWidget*, int32> ShowSequence;
    int32 ShowCounter = 0;

    TArray<FWorldNotificationStruct> PendingQueue;

    /** Live notification per entry (coalesce matching + re-display). */
    TMap<UNotificationToastWidget*, FWorldNotificationStruct> ActiveNotif;
    /** Last show time per entry (coalesce window, perf-counter seconds). */
    TMap<UNotificationToastWidget*, double> ActiveTime;
    /** Total merges since construction (self-test counter). */
    int32 CoalescedCount = 0;
};
