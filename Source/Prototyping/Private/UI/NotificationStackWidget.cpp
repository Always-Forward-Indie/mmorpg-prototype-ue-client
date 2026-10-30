#include "UI/NotificationStackWidget.h"
#include "UI/NotificationToastWidget.h"
#include "Audio/AudioManager.h"
#include "MyGameInstance.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"

void UNotificationStackWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!StackBox)
    {
        StackBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ToastStack"));
        WidgetTree->RootWidget = StackBox;
    }
}

void UNotificationStackWidget::AnchorTopRight()
{
    if (UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this))
    {
        CanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
        CanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
        CanvasSlot->SetPosition(FVector2D(-16.0f, 16.0f));
        CanvasSlot->SetAutoSize(true);
    }
}

void UNotificationStackWidget::ShowNotification(const FWorldNotificationStruct& Notification)
{
    if (UNotificationToastWidget* Entry = AcquireEntry())
    {
        ShowSequence.Add(Entry, ++ShowCounter);
        const EShopStatusSeverity Severity = SeverityForType(Notification.notificationType);
        Entry->DisplayDuration = DisplayDuration;
        Entry->SetSeverity(Severity);
        Entry->ShowNotification(Notification);
        PlaySeveritySound(Severity);
        ReorderNewestFirst();
        return;
    }
    if (PendingQueue.Num() >= MaxQueueSize)
    {
        PendingQueue.RemoveAt(0);
    }
    PendingQueue.Add(Notification);
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

UNotificationToastWidget* UNotificationStackWidget::AcquireEntry()
{
    for (UNotificationToastWidget* Entry : Entries)
    {
        if (IsValid(Entry) && !Entry->IsShowing()
            && Entry->GetVisibility() == ESlateVisibility::Collapsed)
        {
            return Entry;
        }
    }
    if (!EntryClass || Entries.Num() >= MaxVisible)
    {
        return nullptr;
    }
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UNotificationToastWidget* Fresh = CreateWidget<UNotificationToastWidget>(PC, EntryClass))
        {
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
            return Fresh;
        }
    }
    return nullptr;
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
