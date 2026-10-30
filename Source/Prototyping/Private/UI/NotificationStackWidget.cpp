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

void UNotificationStackWidget::ShowNotification(const FWorldNotificationStruct& Notification)
{
    EnsureStackBox();
    if (!StackBox || !RootCanvas)
    {
        UE_LOG(LogGameplay, Error, TEXT("NotificationStack: no visual tree for '%s' — toast invisible"),
            *Notification.notificationType);
        return;
    }
    if (UNotificationToastWidget* Entry = AcquireEntry())
    {
        ShowSequence.Add(Entry, ++ShowCounter);
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
    FWorldNotificationStruct Notif;
    Notif.notificationType = NotificationType;
    Notif.channel          = TEXT("toast");
    Notif.priority         = TEXT("medium");
    Notif.dataFields       = DataFields;
    ShowNotification(Notif);
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
