#include "UI/TradeWidget.h"
#include "Prototyping.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationStackWidget.h"
#include "Audio/AudioManager.h"
#include "Utils/NetCore/TradeStatusRule.h"
#include "Utils/NetCore/PlayerSocialRule.h"
#include "Gameplay/Items/InventoryManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

// S5b: trade chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "Trade"

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UTradeWidget::HandleTradeInviteReceived(const FTradeInviteData& Invite)
{
    // Standalone UTradeInviteWidget dialog is the invite UI: toast + sound
    // here, the window itself opens only on tradeState (accept).
    const FString FromName = !Invite.fromCharacterName.IsEmpty() ? Invite.fromCharacterName : Invite.initiatorName;
    // Toast + sound: the banner alone is easy to miss behind other windows.
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (GI->AudioManager)
            GI->AudioManager->PlayUISound(EUISoundEvent::Notification);
        if (ABasicPlayer* Pawn = GetOwningPlayerPawn<ABasicPlayer>())
        {
            if (UUIManager* UIMgr = Pawn->GetUIManager())
            {
                if (UWorldNotificationManager* NotifMgr = UIMgr->GetWorldNotificationManager())
                {
                    if (UNotificationStackWidget* Stack = NotifMgr->GetToastWidget())
                    {
                        TMap<FString, FString> Fields;
                        Fields.Add(TEXT("fromName"), FromName);
                        Stack->EnqueueActionToast(TEXT("trade_invite"), Fields);
                    }
                }
            }
        }
    }
    // NOTE: the standalone UTradeInviteWidget is the primary invite UI — the
    // trade window itself opens only on tradeState (accept). The banner above
    // stays as a fallback for the open-window case.
}

void UTradeWidget::HandleTradeStateUpdated(const FTradeStateData& State)
{
    const bool bWasConfirmed = CurrentState.myConfirmed;
    CurrentState = State;
    if (GetVisibility() != ESlateVisibility::Visible) OpenTrade();
    // Offer changed under us: our own edit consumes the flag silently,
    // a partner edit unconfirms us and deserves an explicit nudge.
    if (bWasConfirmed && !State.myConfirmed)
    {
        if (bOwnOfferPending)
            bOwnOfferPending = false;
        else
            ShowTradeHint(TEXT("Offer changed"), TEXT("Your partner changed the offer — confirm again."));
    }
    RefreshTradeState();
}

void UTradeWidget::HandleTradeDeclined(const FTradeDeclinedData& Data)
{
    UE_LOG(LogGameplay, Warning, TEXT("TradeWidget: Trade declined by %s"), *Data.byCharacterName);
    CloseTrade();
}

void UTradeWidget::HandleTradeCancelled(const FTradeCancelledData& Data)
{
    const bool bHasSession = !CurrentState.sessionId.IsEmpty();
    const bool bOurs = !Data.sessionId.IsEmpty() && Data.sessionId == CurrentState.sessionId;
    if (bHasSession && !bOurs)
    {
        // Foreign echo (already_in_trade / target_busy on a second request
        // while busy): toast only, the live session and window stay open.
        FString Body = Data.reason;
        if (Body.Equals(TEXT("already_in_trade"), ESearchCase::IgnoreCase))
            Body = TEXT("You are already in a trade.");
        else if (Body.Equals(TEXT("target_busy"), ESearchCase::IgnoreCase))
            Body = TEXT("That player is already trading.");
        UE_LOG(LogGameplay, Warning, TEXT("TradeWidget: foreign cancel echo ignored (%s)"), *Data.reason);
        if (!Body.IsEmpty())
            ShowTradeHint(TEXT("Trade busy"), Body);
        return;
    }
    if (!bHasSession)
    {
        // Stray cancel with nothing open — log only, no toast spam.
        UE_LOG(LogGameplay, Verbose, TEXT("TradeWidget: stray cancel ignored (%s)"), *Data.reason);
        return;
    }
    UE_LOG(LogGameplay, Warning, TEXT("TradeWidget: Trade cancelled"));
    if (!Data.reason.IsEmpty())
        ShowTradeHint(TEXT("Trade cancelled"), Data.reason);
    bOwnOfferPending = false;
    bWarnedTooFar = false;
    CloseTrade();
}

void UTradeWidget::HandleTradeOfferError(const FString& Reason)
{
    // Soft rejection: the session is alive, only this update was refused.
    // Roll the pending offer back to the last server-accepted state so the
    // next send does not re-offer the same bad stack, and stay open.
    UE_LOG(LogGameplay, Warning, TEXT("TradeWidget: offer rejected (%s), rolling back"), *Reason);
    FString Body = Reason.IsEmpty() ? TEXT("invalid item") : Reason;
    if (Body.Equals(TEXT("already_in_trade"), ESearchCase::IgnoreCase))
        Body = TEXT("You are already in a trade.");
    else if (Body.Equals(TEXT("target_busy"), ESearchCase::IgnoreCase))
        Body = TEXT("That player is already trading.");
    ShowTradeHint(TEXT("Offer rejected"), Body);

    PendingOffer.Reset();
    for (const FInventoryItemStruct& Accepted : CurrentState.myItems)
    {
        if (Accepted.id <= 0 || Accepted.quantity <= 0) continue;
        FTradeOfferItem Entry;
        Entry.inventoryItemId = Accepted.id;
        Entry.itemId = Accepted.itemId;
        Entry.quantity = Accepted.quantity;
        PendingOffer.Add(Entry);
    }
    PendingGold = CurrentState.myGold;
    bOwnOfferPending = false;
    RebuildInventoryList();
}

void UTradeWidget::HandleTradeCompleted(const FTradeCompleteData& Data)
{
    UE_LOG(LogGameplay, Warning, TEXT("TradeWidget: Trade completed, session %s"), *Data.sessionId);
    // Completion receipts go to the toast stack (the window closes immediately,
    // so Status_Text-style feedback would never be seen).
    if (Data.bSuccess)
    {
        const APlayerController* PC = GetOwningPlayer();
        const ABasicPlayer* Pawn = PC ? Cast<ABasicPlayer>(PC->GetPawn()) : nullptr;
        UUIManager* UIMgr = Pawn ? Pawn->GetUIManager() : nullptr;
        UWorldNotificationManager* NotifMgr = UIMgr ? UIMgr->GetWorldNotificationManager() : nullptr;
        if (UNotificationStackWidget* Stack = NotifMgr ? NotifMgr->GetToastWidget() : nullptr)
        {
            for (const FTradeReceivedItem& Item : Data.receivedItems)
            {
                TMap<FString, FString> Fields;
                Fields.Add(TEXT("itemId"), FString::FromInt(Item.itemId));
                Fields.Add(TEXT("itemSlug"), Item.slug);
                Fields.Add(TEXT("quantity"), FString::FromInt(Item.quantity));
                Stack->EnqueueActionToast(TEXT("item_received"), Fields);
            }
            if (Data.receivedGold > 0)
            {
                TMap<FString, FString> GoldFields;
                GoldFields.Add(TEXT("amount"), FString::FromInt(Data.receivedGold));
                Stack->EnqueueActionToast(TEXT("gold_received"), GoldFields);
            }
        }
    }
    CloseTrade();
}

void UTradeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // Hover tooltip follows the cursor while visible (all our tooltips
    // position once at show; trade rows are dense, so a stuck tooltip hides
    // the neighbours — follow instead).
    if (TradeTooltip && TradeTooltip->IsVisible())
    {
        if (UWorld* World = GetWorld())
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                FVector2D Mouse = FVector2D::ZeroVector;
                PC->GetMousePosition(Mouse.X, Mouse.Y);
                TradeTooltip->UpdateTooltipPosition(Mouse);
            }
        }
    }
    if (GetVisibility() != ESlateVisibility::Visible || !TradeManager || !TradeManager->IsInTrade()) return;
    if (UWorld* World = GetWorld())
        TickTradeDistance(World->GetTimeSeconds());
}

void UTradeWidget::TickTradeDistance(double NowSeconds)
{
    if (NowSeconds - LastDistCheckTime < 0.5) return;
    LastDistCheckTime = NowSeconds;

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    ABasicPlayer* Local = GI ? GI->Player : nullptr;
    ABasicPlayer* Partner = GI ? GI->GetPlayerByCharacterId(CurrentState.partnerCharacterId) : nullptr;
    if (!Local || !Partner) return;

    const bool bTooFar = FVector::Dist(Local->GetActorLocation(), Partner->GetActorLocation())
        > FPlayerSocialRule::TradeRangeCm;
    if (bTooFar && !bWarnedTooFar)
    {
        bWarnedTooFar = true;
        ShowTradeHint(TEXT("Partner too far"), TEXT("Walk closer or the trade will fail."));
    }
    else if (!bTooFar)
    {
        bWarnedTooFar = false;
    }
}

void UTradeWidget::ShowTradeHint(const FString& Title, const FString& Body)
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) return;
    if (ABasicPlayer* Pawn = GetOwningPlayerPawn<ABasicPlayer>())
    {
        if (UUIManager* UIMgr = Pawn->GetUIManager())
        {
            if (UWorldNotificationManager* NotifMgr = UIMgr->GetWorldNotificationManager())
            {
                if (UNotificationStackWidget* Stack = NotifMgr->GetToastWidget())
                {
                    TMap<FString, FString> Fields;
                    Fields.Add(TEXT("title"), Title);
                    Fields.Add(TEXT("body"), Body);
                    Stack->EnqueueActionToast(TEXT("hint"), Fields);
                }
            }
        }
    }
    if (GI->AudioManager)
        GI->AudioManager->PlayUISound(EUISoundEvent::Notification);
}

void UTradeWidget::HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory)
{
    CachedInventory = Inventory;
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        RebuildInventoryList();
    }
}

void UTradeWidget::HandleConfirmClicked()
{
    if (!TradeManager) return;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    TradeManager->ConfirmTrade(CharId);
}

void UTradeWidget::HandleCancelClicked()
{
    if (!TradeManager) return;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    TradeManager->CancelTrade(CharId);
    CloseTrade();
}

void UTradeWidget::HandleCloseButtonClicked()
{
    HandleCancelClicked();
}

#undef LOCTEXT_NAMESPACE
