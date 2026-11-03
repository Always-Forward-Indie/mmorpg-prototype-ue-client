#include "UI/TradeWidget.h"
#include "Prototyping.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationStackWidget.h"
#include "Audio/AudioManager.h"
#include "Utils/NetCore/TradeStatusRule.h"
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
// UTradeWidget
// ---------------------------------------------------------------------------

void UTradeWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Confirm_Btn)        Confirm_Btn       ->OnClicked.AddDynamic(this, &UTradeWidget::HandleConfirmClicked);
    if (Cancel_Btn)         Cancel_Btn        ->OnClicked.AddDynamic(this, &UTradeWidget::HandleCancelClicked);
    if (Close_Button)       Close_Button      ->OnClicked.AddDynamic(this, &UTradeWidget::HandleCloseButtonClicked);

    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        const float InitScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
        const FVector2D VPSizeInit = FVector2D(W, H) / InitScale;
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D(
            FMath::Max(0.f, (VPSizeInit.X - Size.X) * 0.5f),
            FMath::Max(0.f, (VPSizeInit.Y - Size.Y) * 0.5f));
        SetPositionInViewport(CurrentViewportPosition, false);
    }

    SetVisibility(ESlateVisibility::Collapsed);

    if (Gold_Input)
    {
        Gold_Input->OnTextChanged.AddDynamic(this, &UTradeWidget::HandleGoldTextChanged);
        // Commit (Enter / unfocus) sends the gold offer — typing alone never does.
        Gold_Input->OnTextCommitted.AddDynamic(this, &UTradeWidget::HandleGoldTextCommitted);
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.AddDynamic(this, &UTradeWidget::HandleLocaleChanged);
        }
    }
}

void UTradeWidget::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.RemoveDynamic(this, &UTradeWidget::HandleLocaleChanged);
        }
    }
    Super::NativeDestruct();
}

void UTradeWidget::BindToManagers(UTradeManager* InTradeManager, UInventoryManager* InInventoryManager)
{
    if (!InTradeManager) return;

    if (TradeManager)
    {
        TradeManager->OnTradeInviteReceivedDelegate.RemoveDynamic(this, &UTradeWidget::HandleTradeInviteReceived);
        TradeManager->OnTradeStateUpdatedDelegate  .RemoveDynamic(this, &UTradeWidget::HandleTradeStateUpdated);
        TradeManager->OnTradeDeclinedDelegate      .RemoveDynamic(this, &UTradeWidget::HandleTradeDeclined);
        TradeManager->OnTradeCancelledDelegate     .RemoveDynamic(this, &UTradeWidget::HandleTradeCancelled);
        TradeManager->OnTradeCompletedDelegate     .RemoveDynamic(this, &UTradeWidget::HandleTradeCompleted);
        TradeManager->OnTradeOfferErrorDelegate    .RemoveDynamic(this, &UTradeWidget::HandleTradeOfferError);
    }
    if (InventoryManager)
    {
        InventoryManager->OnInventoryUpdated.RemoveDynamic(this, &UTradeWidget::HandleInventoryUpdated);
    }

    TradeManager    = InTradeManager;
    InventoryManager = InInventoryManager;

    TradeManager->OnTradeInviteReceivedDelegate.AddDynamic(this, &UTradeWidget::HandleTradeInviteReceived);
    TradeManager->OnTradeStateUpdatedDelegate  .AddDynamic(this, &UTradeWidget::HandleTradeStateUpdated);
    TradeManager->OnTradeDeclinedDelegate      .AddDynamic(this, &UTradeWidget::HandleTradeDeclined);
    TradeManager->OnTradeCancelledDelegate     .AddDynamic(this, &UTradeWidget::HandleTradeCancelled);
    TradeManager->OnTradeCompletedDelegate     .AddDynamic(this, &UTradeWidget::HandleTradeCompleted);
    TradeManager->OnTradeOfferErrorDelegate    .AddDynamic(this, &UTradeWidget::HandleTradeOfferError);

    if (InventoryManager)
    {
        InventoryManager->OnInventoryUpdated.AddDynamic(this, &UTradeWidget::HandleInventoryUpdated);
        CachedInventory = InventoryManager->GetInventory();
    }
}

void UTradeWidget::OpenTrade()
{
    PendingOffer.Reset();
    PendingGold = 0;
    bOwnOfferPending = false;
    bWarnedTooFar = false;
    if (Gold_Input)
        Gold_Input->SetText(FText::FromString(TEXT("0")));
    RebuildInventoryList();
    SetVisibility(ESlateVisibility::Visible);
    OnTradeVisibilityChanged.Broadcast(true);
}

void UTradeWidget::CloseTrade()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnTradeVisibilityChanged.Broadcast(false);
}

void UTradeWidget::RefreshTradeState()
{
    if (!TradeManager) return;
    CurrentState = TradeManager->GetCurrentState();

    RebuildOfferList(My_Items_Box, MyOfferSlots, CurrentState.myItems, true);
    RebuildOfferList(Their_Items_Box, TheirOfferSlots, CurrentState.theirItems, false);

    // Pick-list badges show remaining (owned − offered): rebuild on every
    // echo too, or the counts go stale until the next inventory push.
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        RebuildInventoryList();
    }

    if (My_Gold_Text)    My_Gold_Text   ->SetText(FText::Format(LOCTEXT("GoldValue", "{0} g"), FText::AsNumber(CurrentState.myGold)));
    if (Their_Gold_Text) Their_Gold_Text->SetText(FText::Format(LOCTEXT("GoldValue", "{0} g"), FText::AsNumber(CurrentState.theirGold)));

    const FText ConfirmedStr = LOCTEXT("Confirmed", "Confirmed");
    const FText PendingStr   = LOCTEXT("Pending", "Pending");
    const FLinearColor ConfirmedCol(0.35f, 1.0f, 0.45f);
    const FLinearColor PendingCol(0.65f, 0.65f, 0.65f);

    if (My_Confirm_Text)
    {
        My_Confirm_Text->SetText(CurrentState.myConfirmed ? ConfirmedStr : PendingStr);
        My_Confirm_Text->SetColorAndOpacity(FSlateColor(CurrentState.myConfirmed ? ConfirmedCol : PendingCol));
    }
    if (Their_Confirm_Text)
    {
        const FText TheirStatus = CurrentState.theirConfirmed ? ConfirmedStr : PendingStr;
        if (!CurrentState.partnerName.IsEmpty())
            Their_Confirm_Text->SetText(FText::Format(LOCTEXT("PartnerConfirmFmt", "{0}: {1}"),
                FText::FromString(CurrentState.partnerName), TheirStatus));
        else
            Their_Confirm_Text->SetText(TheirStatus);
        Their_Confirm_Text->SetColorAndOpacity(FSlateColor(CurrentState.theirConfirmed ? ConfirmedCol : PendingCol));
    }

    // Sync the input box to the accepted state (e.g. after an error rollback),
    // but never fight the user mid-typing.
    if (Gold_Input && !Gold_Input->HasKeyboardFocus())
    {
        const FString AcceptedStr = FString::FromInt(CurrentState.myGold);
        if (Gold_Input->GetText().ToString() != AcceptedStr)
            Gold_Input->SetText(FText::FromString(AcceptedStr));
    }

    // Disable confirm once we confirmed (waiting for server complete);
    // peer flag never gates us. Single owner: FTradeStatusRule.
    if (Confirm_Btn)
        Confirm_Btn->SetIsEnabled(FTradeStatusRule::CanConfirm(CurrentState.sessionId, CurrentState.myConfirmed));
}

#undef LOCTEXT_NAMESPACE
