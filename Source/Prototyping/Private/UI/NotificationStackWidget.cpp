#include "UI/NotificationStackWidget.h"
#include "UI/NotificationToastWidget.h"
#include "Audio/AudioManager.h"
#include "MyGameInstance.h"
#include "Prototyping.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UNotificationStackWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // THE visual-tree hook: RebuildWidget() consumes WidgetTree->RootWidget
    // BEFORE NativeConstruct() ever runs, so a tree built in NativeConstruct
    // renders nothing forever (silent blackout, zero errors). Initializing
    // here guarantees the root exists before Slate is built.
    EnsureStackBox();
}

void UNotificationStackWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EnsureStackBox();
    UE_LOG(LogGameplay, Warning, TEXT("NotificationStack: ready (entries=%s, max=%d)"),
        EntryClass ? *EntryClass->GetName() : TEXT("NONE"), MaxVisible);
}

void UNotificationStackWidget::EnsureStackBox()
{
    if (!WidgetTree) return;
    if (!RootCanvas)
    {
        RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ToastRoot"));
        WidgetTree->RootWidget = RootCanvas;
    }
    if (RootCanvas && !StackBox)
    {
        StackBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ToastStack"));
        if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(StackBox))
        {
            CanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
            CanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
            CanvasSlot->SetPosition(FVector2D(-16.0f, 16.0f));
            CanvasSlot->SetAutoSize(true);
        }
    }
}

void UNotificationStackWidget::AnchorTopRight()
{
    // The canvas slot is pinned in EnsureStackBox; re-assert in case the
    // widget was re-parented after construction.
    EnsureStackBox();
}

void UNotificationStackWidget::GetToastDiagnostics(int32& OutTotalEntries, int32& OutVisibleEntries,
    int32& OutQueued, bool& OutTreeBuilt) const
{
    OutTotalEntries = 0;
    OutVisibleEntries = 0;
    for (UNotificationToastWidget* Entry : Entries)
    {
        if (!IsValid(Entry)) continue;
        ++OutTotalEntries;
        if (Entry->IsShowing() && Entry->GetVisibility() != ESlateVisibility::Collapsed
            && !Entry->GetCurrentBody().IsEmpty())
        {
            ++OutVisibleEntries;
        }
    }
    OutQueued = PendingQueue.Num();
    OutTreeBuilt = (WidgetTree && WidgetTree->RootWidget == RootCanvas && RootCanvas && StackBox);
}

void UNotificationStackWidget::GetVisibleToastBodies(TArray<FString>& OutBodies) const
{
    OutBodies.Reset();
    for (UNotificationToastWidget* Entry : Entries)
    {
        if (!IsValid(Entry) || !Entry->IsShowing()) continue;
        OutBodies.Add(Entry->GetCurrentBody().ToString());
    }
}

void UNotificationStackWidget::ShowNotification(const FWorldNotificationStruct& Notification)
{
    EnsureStackBox();
    if (!StackBox || !RootCanvas)
    {
        UE_LOG(LogGameplay, Error, TEXT("NotificationStack: no visual tree for '%s' — toast invisible"),
            *Notification.notificationType);
        return;
    }
    // Anti-spam first: identical gains inside the window refresh one entry.
    if (TryCoalesce(Notification))
    {
        return;
    }
    if (UNotificationToastWidget* Entry = AcquireEntry())
    {
        ShowSequence.Add(Entry, ++ShowCounter);
        ActiveNotif.Add(Entry, Notification);
        ActiveTime.Add(Entry, FPlatformTime::Seconds());
        const EShopStatusSeverity Severity = SeverityForType(Notification.notificationType);
        Entry->DisplayDuration = DisplayDuration;
        Entry->SetSeverity(Severity);
        Entry->ShowNotification(Notification);
        PlaySeveritySound(Severity);
        ReorderNewestFirst();
        UE_LOG(LogGameplay, Verbose, TEXT("NotificationStack: showing '%s' (visible=%d, queued=%d)"),
            *Notification.notificationType, Entries.Num(), PendingQueue.Num());
        return;
    }
    if (PendingQueue.Num() >= MaxQueueSize)
    {
        PendingQueue.RemoveAt(0);
    }
    PendingQueue.Add(Notification);
    UE_LOG(LogGameplay, Warning, TEXT("NotificationStack: pool exhausted, queued '%s' (queued=%d)"),
        *Notification.notificationType, PendingQueue.Num());
}

void UNotificationStackWidget::EnqueueActionToast(const FString& NotificationType, const TMap<FString, FString>& DataFields)
{
    // Zero-amount tripwire: a gain toast carrying nothing names its cause.
    // (The vendor floor guarantees >= 1g server-side; a 0 here means a
    // zero-price item or a broken amount field upstream.)
    if (FToastCoalesceRule::IsGainType(NotificationType))
    {
        const FString AmountField = FToastCoalesceRule::AmountField(NotificationType);
        if (!AmountField.IsEmpty())
        {
            const FString* AmountPtr = DataFields.Find(AmountField);
            if (AmountPtr && FCString::Atoi(**AmountPtr) <= 0)
            {
                FString Dump;
                for (const auto& Pair : DataFields)
                    Dump += FString::Printf(TEXT("[%s=%s]"), *Pair.Key, *Pair.Value);
                UE_LOG(LogGameplay, Warning, TEXT("NotificationStack: '%s' carries %s<=0: %s"),
                    *NotificationType, *AmountField, *Dump);
            }
        }
    }
    FWorldNotificationStruct Notif;
    Notif.notificationType = NotificationType;
    Notif.channel          = TEXT("toast");
    Notif.priority         = TEXT("medium");
    Notif.dataFields       = DataFields;
    ShowNotification(Notif);
}

bool UNotificationStackWidget::TryCoalesce(const FWorldNotificationStruct& Notification)
{
    const FString Key = FToastCoalesceRule::CoalesceKey(Notification.notificationType, Notification.dataFields);
    if (Key.IsEmpty()) return false;
    const FString Field = FToastCoalesceRule::AmountField(Notification.notificationType);
    if (Field.IsEmpty()) return false;
    const double Now = FPlatformTime::Seconds();
    const float Window = FToastCoalesceRule::CoalesceWindowSec();

    // Visible entries first: merge and refresh the one entry (no extra sound).
    for (auto& Pair : ActiveNotif)
    {
        UNotificationToastWidget* Entry = Pair.Key;
        if (!IsValid(Entry) || !Entry->IsShowing()) continue;
        FWorldNotificationStruct& Live = Pair.Value;
        if (Live.notificationType != Notification.notificationType) continue;
        if (FToastCoalesceRule::CoalesceKey(Live.notificationType, Live.dataFields) != Key) continue;
        const double* ShownAt = ActiveTime.Find(Entry);
        if (!ShownAt || (Now - *ShownAt) > Window) continue;
        const int32 Merged = FToastCoalesceRule::MergeAmounts(
            Live.dataFields.FindRef(Field), Notification.dataFields.FindRef(Field));
        Live.dataFields.Add(Field, FString::FromInt(Merged));
        Entry->ShowNotification(Live);
        ShowSequence.Add(Entry, ++ShowCounter);
        ActiveTime.Add(Entry, Now);
        ++CoalescedCount;
        ReorderNewestFirst();
        UE_LOG(LogGameplay, Verbose, TEXT("NotificationStack: coalesced '%s' (%s=%d)"),
            *Notification.notificationType, *Field, Merged);
        return true;
    }

    // Overflow queue: merge in place so a burst behind a full pool still
    // collapses to one promotion.
    for (FWorldNotificationStruct& Queued : PendingQueue)
    {
        if (Queued.notificationType != Notification.notificationType) continue;
        if (FToastCoalesceRule::CoalesceKey(Queued.notificationType, Queued.dataFields) != Key) continue;
        const int32 Merged = FToastCoalesceRule::MergeAmounts(
            Queued.dataFields.FindRef(Field), Notification.dataFields.FindRef(Field));
        Queued.dataFields.Add(Field, FString::FromInt(Merged));
        ++CoalescedCount;
        UE_LOG(LogGameplay, Verbose, TEXT("NotificationStack: coalesced queued '%s' (%s=%d)"),
            *Notification.notificationType, *Field, Merged);
        return true;
    }
    return false;
}

EShopStatusSeverity UNotificationStackWidget::SeverityForType(const FString& NotificationType)
{
    if (NotificationType.Contains(TEXT("fail")) || NotificationType.Contains(TEXT("error")))
    {
        return EShopStatusSeverity::Error;
    }
    if (NotificationType.Contains(TEXT("received")) || NotificationType.Contains(TEXT("learned"))
        || NotificationType.Contains(TEXT("unlocked")) || NotificationType.Contains(TEXT("explored"))
        || NotificationType.Contains(TEXT("tier_up")) || NotificationType.Contains(TEXT("turned_in"))
        || NotificationType.Contains(TEXT("offered")))
    {
        return EShopStatusSeverity::Success;
    }
    return EShopStatusSeverity::Info;
}

void UNotificationStackWidget::PlaySeveritySound(EShopStatusSeverity Severity)
{
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (GI->AudioManager)
        {
            GI->AudioManager->PlayUISound(Severity == EShopStatusSeverity::Error
                ? EUISoundEvent::ErrorSound
                : EUISoundEvent::Notification);
        }
    }
}

APlayerController* UNotificationStackWidget::ResolvePlayerController() const
{
    // Never trust a cached owning player across level transitions: resolve
    // from the live world every time an entry is spawned.
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
            return PC;
    }
    return GetOwningPlayer();
}

UNotificationToastWidget* UNotificationStackWidget::AcquireEntry()
{
    for (int32 i = 0; i < Entries.Num(); ++i)
    {
        UNotificationToastWidget* Entry = Entries[i];
        if (IsValid(Entry) && !Entry->IsShowing()
            && Entry->GetVisibility() == ESlateVisibility::Collapsed)
        {
            UE_LOG(LogGameplay, Verbose, TEXT("NotificationStack: reusing entry %d (pool=%d)"),
                i, Entries.Num());
            return Entry;
        }
    }
    if (!EntryClass || Entries.Num() >= MaxVisible)
    {
        UE_LOG(LogGameplay, Warning, TEXT("NotificationStack: pool exhausted (class=%s total=%d max=%d)"),
            EntryClass ? *EntryClass->GetName() : TEXT("NONE"), Entries.Num(), MaxVisible);
        return nullptr;
    }
    APlayerController* PC = ResolvePlayerController();
    if (!PC)
    {
        UE_LOG(LogGameplay, Warning, TEXT("NotificationStack: no player controller for new entry"));
        return nullptr;
    }
    UNotificationToastWidget* Fresh = CreateWidget<UNotificationToastWidget>(PC, EntryClass);
    if (!Fresh)
    {
        UE_LOG(LogGameplay, Warning, TEXT("NotificationStack: CreateWidget failed (class=%s)"),
            EntryClass ? *EntryClass->GetName() : TEXT("NONE"));
        return nullptr;
    }
    UE_LOG(LogGameplay, Verbose, TEXT("NotificationStack: spawned entry %d"), Entries.Num() + 1);
    Fresh->DisplayDuration = DisplayDuration;
    Fresh->OnToastExpired.AddUObject(this, &UNotificationStackWidget::HandleEntryExpired);
    Entries.Add(Fresh);
    if (StackBox)
    {
        if (UVerticalBoxSlot* BoxSlot = StackBox->AddChildToVerticalBox(Fresh))
        {
            BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
            BoxSlot->SetHorizontalAlignment(HAlign_Right);
        }
    }
    UE_LOG(LogGameplay, Verbose, TEXT("NotificationStack: spawned entry %d"), Entries.Num());
    return Fresh;
}

void UNotificationStackWidget::PromoteNext()
{
    if (PendingQueue.Num() == 0) return;
    if (UNotificationToastWidget* Entry = AcquireEntry())
    {
        const FWorldNotificationStruct Next = PendingQueue[0];
        PendingQueue.RemoveAt(0);
        ShowSequence.Add(Entry, ++ShowCounter);
        ActiveNotif.Add(Entry, Next);
        ActiveTime.Add(Entry, FPlatformTime::Seconds());
        const EShopStatusSeverity Severity = SeverityForType(Next.notificationType);
        Entry->DisplayDuration = DisplayDuration;
        Entry->SetSeverity(Severity);
        Entry->ShowNotification(Next);
        PlaySeveritySound(Severity);
        ReorderNewestFirst();
    }
}

void UNotificationStackWidget::HandleEntryExpired(UNotificationToastWidget* ExpiredEntry)
{
    if (IsValid(ExpiredEntry))
    {
        ExpiredEntry->SetVisibility(ESlateVisibility::Collapsed);
    }
    ActiveNotif.Remove(ExpiredEntry);
    ActiveTime.Remove(ExpiredEntry);
    PromoteNext();
    ReorderNewestFirst();
}

void UNotificationStackWidget::ReorderNewestFirst()
{
    // Newest shown entry sits on top (ShowSequence order). Pool is tiny
    // (<= MaxVisible), rebuild cost is negligible.
    if (!StackBox) return;
    TArray<UNotificationToastWidget*> Visible;
    TArray<UNotificationToastWidget*> Hidden;
    for (UNotificationToastWidget* Entry : Entries)
    {
        if (!IsValid(Entry)) continue;
        if (Entry->IsShowing())
            Visible.Add(Entry);
        else
            Hidden.Add(Entry);
    }
    Visible.Sort([this](const UNotificationToastWidget& A, const UNotificationToastWidget& B)
    {
        const int32 SeqA = ShowSequence.FindRef(const_cast<UNotificationToastWidget*>(&A));
        const int32 SeqB = ShowSequence.FindRef(const_cast<UNotificationToastWidget*>(&B));
        return SeqA > SeqB;
    });
    StackBox->ClearChildren();
    for (UNotificationToastWidget* Entry : Visible)
    {
        if (UVerticalBoxSlot* BoxSlot = StackBox->AddChildToVerticalBox(Entry))
        {
            BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
            BoxSlot->SetHorizontalAlignment(HAlign_Right);
        }
    }
    for (UNotificationToastWidget* Entry : Hidden)
    {
        Entry->SetVisibility(ESlateVisibility::Collapsed);
        if (UVerticalBoxSlot* BoxSlot = StackBox->AddChildToVerticalBox(Entry))
        {
            BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
            BoxSlot->SetHorizontalAlignment(HAlign_Right);
        }
    }
}
