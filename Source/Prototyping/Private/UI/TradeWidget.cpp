#include "UI/TradeWidget.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"

// ---------------------------------------------------------------------------
// UTradeInvItemRowBinding
// ---------------------------------------------------------------------------

void UTradeInvItemRowBinding::Setup(UTradeWidget* InWidget, int32 InInventoryItemId, int32 InItemId)
{
    Widget          = InWidget;
    InventoryItemId = InInventoryItemId;
    ItemId          = InItemId;
}

void UTradeInvItemRowBinding::HandleClicked()
{
    if (Widget) Widget->DispatchAddItem(InventoryItemId, ItemId);
}

// ---------------------------------------------------------------------------
// UTradeWidget
// ---------------------------------------------------------------------------

void UTradeWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Confirm_Btn)        Confirm_Btn       ->OnClicked.AddDynamic(this, &UTradeWidget::HandleConfirmClicked);
    if (Cancel_Btn)         Cancel_Btn        ->OnClicked.AddDynamic(this, &UTradeWidget::HandleCancelClicked);
    if (Close_Button)       Close_Button      ->OnClicked.AddDynamic(this, &UTradeWidget::HandleCloseButtonClicked);
    if (Invite_Accept_Btn)  Invite_Accept_Btn ->OnClicked.AddDynamic(this, &UTradeWidget::HandleInviteAcceptClicked);
    if (Invite_Decline_Btn) Invite_Decline_Btn->OnClicked.AddDynamic(this, &UTradeWidget::HandleInviteDeclineClicked);

    if (Invite_Banner) Invite_Banner->SetVisibility(ESlateVisibility::Collapsed);

    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        int32 W = 0, H = 0;
        PC->GetViewportSize(W, H);
        ForceLayoutPrepass();
        const FVector2D Size = GetDesiredSize();
        CurrentViewportPosition = FVector2D((W - Size.X) * 0.5f, (H - Size.Y) * 0.5f);
        SetPositionInViewport(CurrentViewportPosition, false);
    }

    SetVisibility(ESlateVisibility::Collapsed);
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
    RebuildInventoryList();
    SetVisibility(ESlateVisibility::Visible);
    OnTradeVisibilityChanged.Broadcast(true);
}

void UTradeWidget::CloseTrade()
{
    SetVisibility(ESlateVisibility::Collapsed);
    OnTradeVisibilityChanged.Broadcast(false);
    if (Invite_Banner) Invite_Banner->SetVisibility(ESlateVisibility::Collapsed);
}

void UTradeWidget::RefreshTradeState()
{
    if (!TradeManager) return;
    CurrentState = TradeManager->GetCurrentState();

    RebuildItemsList(My_Items_Box,    CurrentState.myItems);
    RebuildItemsList(Their_Items_Box, CurrentState.theirItems);

    if (My_Gold_Text)    My_Gold_Text   ->SetText(FText::FromString(FString::Printf(TEXT("%d g"), CurrentState.myGold)));
    if (Their_Gold_Text) Their_Gold_Text->SetText(FText::FromString(FString::Printf(TEXT("%d g"), CurrentState.theirGold)));

    const FString MyConfirmStr    = CurrentState.myConfirmed    ? TEXT("? Confirmed") : TEXT("Pending");
    const FString TheirConfirmStr = CurrentState.theirConfirmed ? TEXT("? Confirmed") : TEXT("Pending");

    if (My_Confirm_Text)    My_Confirm_Text   ->SetText(FText::FromString(MyConfirmStr));
    if (Their_Confirm_Text) Their_Confirm_Text->SetText(FText::FromString(TheirConfirmStr));

    // Disable confirm while both already confirmed (waiting for server complete)
    if (Confirm_Btn)
        Confirm_Btn->SetIsEnabled(!CurrentState.myConfirmed);
}

void UTradeWidget::DispatchAddItem(int32 InventoryItemId, int32 ItemId)
{
    if (!TradeManager || !TradeManager->IsInTrade()) return;

    // Check if already in offer
    for (const FTradeOfferItem& Existing : PendingOffer)
    {
        if (Existing.inventoryItemId == InventoryItemId) return;
    }

    FTradeOfferItem NewItem;
    NewItem.inventoryItemId = InventoryItemId;
    NewItem.itemId          = ItemId;
    NewItem.quantity        = 1;
    PendingOffer.Add(NewItem);

    // Resolve gold from input field
    if (Gold_Input)
    {
        PendingGold = FCString::Atoi(*Gold_Input->GetText().ToString());
    }

    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    TradeManager->UpdateTradeOffer(CharId, PendingGold, PendingOffer);
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UTradeWidget::HandleTradeInviteReceived(const FTradeInviteData& Invite)
{
    if (Invite_Banner)
    {
        Invite_Banner->SetVisibility(ESlateVisibility::Visible);
    }
    if (Invite_Name_Text)
    {
        Invite_Name_Text->SetText(FText::FromString(
            FString::Printf(TEXT("%s wants to trade"), *Invite.fromCharacterName)));
    }
    // Show the widget so the invite banner is visible
    if (GetVisibility() != ESlateVisibility::Visible)
    {
        SetVisibility(ESlateVisibility::Visible);
        OnTradeVisibilityChanged.Broadcast(true);
    }
}

void UTradeWidget::HandleTradeStateUpdated(const FTradeStateData& State)
{
    CurrentState = State;
    if (Invite_Banner) Invite_Banner->SetVisibility(ESlateVisibility::Collapsed);
    if (GetVisibility() != ESlateVisibility::Visible) OpenTrade();
    RefreshTradeState();
}

void UTradeWidget::HandleTradeDeclined(const FTradeDeclinedData& Data)
{
    UE_LOG(LogTemp, Warning, TEXT("TradeWidget: Trade declined by %s"), *Data.byCharacterName);
    CloseTrade();
}

void UTradeWidget::HandleTradeCancelled(const FTradeCancelledData& Data)
{
    UE_LOG(LogTemp, Warning, TEXT("TradeWidget: Trade cancelled — %s"), *Data.reason);
    CloseTrade();
}

void UTradeWidget::HandleTradeCompleted(const FTradeCompleteData& Data)
{
    UE_LOG(LogTemp, Warning, TEXT("TradeWidget: Trade completed — session %s"), *Data.sessionId);
    CloseTrade();
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

void UTradeWidget::HandleInviteAcceptClicked()
{
    if (!TradeManager) return;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    TradeManager->RespondToTradeInvite(CharId, true);
    if (Invite_Banner) Invite_Banner->SetVisibility(ESlateVisibility::Collapsed);
}

void UTradeWidget::HandleInviteDeclineClicked()
{
    if (!TradeManager) return;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    TradeManager->RespondToTradeInvite(CharId, false);
    CloseTrade();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UTradeWidget::RebuildItemsList(UScrollBox* Box, const TArray<FInventoryItemStruct>& Items)
{
    if (!Box) return;
    Box->ClearChildren();

    for (const FInventoryItemStruct& Item : Items)
    {
        if (!InvRowClass) continue;
        UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), InvRowClass);
        if (!Row) continue;

        if (UTextBlock* Txt = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Name_Text"))))
        {
            FText ItemName = FText::FromString(Item.slug);
            if (!Item.slug.IsEmpty())
            {
                if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        const FText LocName = Loc->GetItemDisplayName(Item.slug);
                        if (!LocName.IsEmpty()) ItemName = LocName;
                    }
                }
            }
            Txt->SetText(FText::FromString(
                FString::Printf(TEXT("%s x%d"), *ItemName.ToString(), Item.quantity)));
        }
        // Hide the add button for offer display rows
        if (UButton* Btn = Cast<UButton>(Row->GetWidgetFromName(TEXT("Row_Add_Btn"))))
        {
            Btn->SetVisibility(ESlateVisibility::Collapsed);
        }
        Box->AddChild(Row);
    }
}

void UTradeWidget::RebuildItemsList(UScrollBox* Box, const TArray<FTradeOfferItem>& Items)
{
    if (!Box) return;
    Box->ClearChildren();

    for (const FTradeOfferItem& Offer : Items)
    {
        if (!InvRowClass) continue;
        UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), InvRowClass);
        if (!Row) continue;

        if (UTextBlock* Txt = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Name_Text"))))
        {
            FText ItemName = FText::FromString(Offer.itemSlug);
            if (!Offer.itemSlug.IsEmpty())
            {
                if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        const FText LocName = Loc->GetItemDisplayName(Offer.itemSlug);
                        if (!LocName.IsEmpty()) ItemName = LocName;
                    }
                }
            }
            Txt->SetText(FText::FromString(
                FString::Printf(TEXT("%s x%d"), *ItemName.ToString(), Offer.quantity)));
        }
        if (UButton* Btn = Cast<UButton>(Row->GetWidgetFromName(TEXT("Row_Add_Btn"))))
        {
            Btn->SetVisibility(ESlateVisibility::Collapsed);
        }
        Box->AddChild(Row);
    }
}

void UTradeWidget::RebuildInventoryList()
{
    if (!Inv_Items_Box) return;

    Inv_Items_Box->ClearChildren();
    InvRowBindings.Reset();

    for (const FInventoryItemStruct& Item : CachedInventory.items)
    {
        if (!InvRowClass) continue;
        UUserWidget* Row = CreateWidget<UUserWidget>(GetOwningPlayer(), InvRowClass);
        if (!Row) continue;

        if (UTextBlock* Txt = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("Row_Name_Text"))))
        {
            FText ItemName = FText::FromString(Item.slug);
            if (!Item.slug.IsEmpty())
            {
                if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
                {
                    if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                    {
                        const FText LocName = Loc->GetItemDisplayName(Item.slug);
                        if (!LocName.IsEmpty()) ItemName = LocName;
                    }
                }
            }
            Txt->SetText(FText::FromString(
                FString::Printf(TEXT("%s x%d"), *ItemName.ToString(), Item.quantity)));
        }
        if (UButton* Btn = Cast<UButton>(Row->GetWidgetFromName(TEXT("Row_Add_Btn"))))
        {
            UTradeInvItemRowBinding* Binding = NewObject<UTradeInvItemRowBinding>(this);
            Binding->Setup(this, Item.id, Item.itemId);
            InvRowBindings.Add(Binding);
            Btn->OnClicked.AddDynamic(Binding, &UTradeInvItemRowBinding::HandleClicked);
        }
        Inv_Items_Box->AddChild(Row);
    }
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

FReply UTradeWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bool bShouldDrag = !DragHandle;
        if (DragHandle)
        {
            const FGeometry G = DragHandle->GetCachedGeometry();
            const FVector2D L = G.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
            const FVector2D S = G.GetLocalSize();
            bShouldDrag = (L.X >= 0 && L.X <= S.X && L.Y >= 0 && L.Y <= S.Y);
        }
        if (bShouldDrag)
        {
            const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
            DragOffset = InMouseEvent.GetScreenSpacePosition() / Scale - CurrentViewportPosition;
            bDragging  = true;
            if (TSharedPtr<SWidget> Slate = GetCachedWidget())
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            return FReply::Handled();
        }
    }
    return FReply::Unhandled();
}

FReply UTradeWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        if (TSharedPtr<SWidget> Slate = GetCachedWidget())
            return FReply::Handled().ReleaseMouseCapture();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply UTradeWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging) { UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition()); return FReply::Handled(); }
    return FReply::Unhandled();
}

void UTradeWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D VP = FVector2D(W, H) / Scale;
    ForceLayoutPrepass();
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(600, 600);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, VP.X - Size.X);
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, VP.Y - Size.Y);
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}
