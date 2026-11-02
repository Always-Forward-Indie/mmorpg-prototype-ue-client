#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Data/DataStructs.h"
#include "UI/VendorSlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "TradeWidget.generated.h"

class UTradeManager;
class UInventoryManager;
class UPanelWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeWidgetVisibilityChanged, bool, bIsVisible);

/**
 * TradeWidget
 *
 * Full P2P trade session UI:
 *   Left panel  � my offer (items + gold I'm giving)
 *   Right panel � their offer (items + gold they're giving)
 *   Confirm / Cancel buttons
 *   Pending invite banner � Accept / Decline
 *
 * Blueprint subclass must bind:
 *   My_Items_Box       UScrollBox   � my offered items
 *   Their_Items_Box    UScrollBox   � partner's offered items
 *   Inv_Items_Box      UScrollBox   � my inventory (items to add to offer)
 *   My_Gold_Text       UTextBlock   � gold I'm offering
 *   Their_Gold_Text    UTextBlock   � gold partner is offering
 *   My_Confirm_Text    UTextBlock   � "Confirmed" / "Pending"
 *   Their_Confirm_Text UTextBlock   � "Confirmed" / "Pending"
 *   Gold_Input         UEditableTextBox � type gold amount (BindWidgetOptional)
 *   Confirm_Btn        UButton
 *   Cancel_Btn         UButton
 *   Close_Button       UButton      (BindWidgetOptional)
 *
 *   Invite_Banner      UWidget      (BindWidgetOptional) � shown on tradeInvite
 *   Invite_Name_Text   UTextBlock   (BindWidgetOptional)
 *   Invite_Accept_Btn  UButton      (BindWidgetOptional)
 *   Invite_Decline_Btn UButton      (BindWidgetOptional)
 *
 * Inventory pick list and both offer panels use UVendorSlotWidget rows
 * (TradeSlotClass — same BP asset as the vendor shop; click pick-list
 * slot = add to offer; hover = tooltip via TradeTooltipWidgetClass).
 * Offer slots hide price and remove-button (trade has neither).
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UTradeWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Trade UI")
    void BindToManagers(UTradeManager* InTradeManager, UInventoryManager* InInventoryManager);

    UFUNCTION(BlueprintCallable, Category = "Trade UI")
    void RefreshTradeState();

    UFUNCTION(BlueprintCallable, Category = "Trade UI")
    void OpenTrade();

    UFUNCTION(BlueprintCallable, Category = "Trade UI")
    void CloseTrade();

    /**
     * Slot widget for the inventory pick list (icons + hover tooltip).
     * Reuse the vendor slot BP (same asset as the shop uses).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade UI")
    TSubclassOf<UVendorSlotWidget> TradeSlotClass;

    /** Row widget for the text offer lists ("name xN"). */
    /** Item tooltip for pick-list and offer hover. Optional — icons work without it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade UI")
    TSubclassOf<UItemTooltipWidget> TradeTooltipWidgetClass;

    UPROPERTY(BlueprintAssignable, Category = "Trade UI Events")
    FOnTradeWidgetVisibilityChanged OnTradeVisibilityChanged;

    // Called by pick-list slots
    void DispatchAddItem(int32 InventoryItemId, int32 ItemId);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    /** Throttled proximity watch: warns once per excursion beyond trade range. */
    void TickTradeDistance(double NowSeconds);

    UFUNCTION() void HandleTradeInviteReceived(const FTradeInviteData& Invite);
    UFUNCTION() void HandleTradeStateUpdated(const FTradeStateData& State);
    UFUNCTION() void HandleTradeDeclined(const FTradeDeclinedData& Data);
    UFUNCTION() void HandleTradeCancelled(const FTradeCancelledData& Data);
    UFUNCTION() void HandleTradeCompleted(const FTradeCompleteData& Data);
    UFUNCTION() void HandleTradeOfferError(const FString& Reason);
    UFUNCTION() void HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory);

    UFUNCTION() void HandleConfirmClicked();
    UFUNCTION() void HandleCancelClicked();
    UFUNCTION() void HandleCloseButtonClicked();
    UFUNCTION() void HandleInviteAcceptClicked();
    UFUNCTION() void HandleInviteDeclineClicked();
    UFUNCTION() void HandleGoldTextChanged(const FText& Text);

    // --- My offer ---
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UScrollBox* My_Items_Box       = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UScrollBox* Their_Items_Box    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UScrollBox* Inv_Items_Box      = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UTextBlock* My_Gold_Text       = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UTextBlock* Their_Gold_Text    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UTextBlock* My_Confirm_Text    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UTextBlock* Their_Confirm_Text = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UButton*    Confirm_Btn        = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UButton*    Cancel_Btn         = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UEditableTextBox* Gold_Input         = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget*          Invite_Banner      = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock*       Invite_Name_Text   = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*          Invite_Accept_Btn  = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*          Invite_Decline_Btn = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*          Close_Button       = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget*          DragHandle         = nullptr;

private:
    // Rebuilds the offered-items display in a trade panel from full inventory item data
    void RebuildOfferList(UPanelWidget* Box, TArray<UVendorSlotWidget*>& OutSlots, const TArray<FInventoryItemStruct>& Items, bool bIsMyOffer);
    void RebuildInventoryList();

    UPROPERTY() UTradeManager*    TradeManager    = nullptr;
    UPROPERTY() UInventoryManager* InventoryManager = nullptr;

    // Slot widget arrays (kept for index lookup)
    UPROPERTY() TArray<UVendorSlotWidget*> TradeInvSlots;
    UPROPERTY() TArray<UVendorSlotWidget*> MyOfferSlots;
    UPROPERTY() TArray<UVendorSlotWidget*> TheirOfferSlots;

    // Pick-list hover tooltip (created in NativeConstruct when assigned).
    UPROPERTY() UItemTooltipWidget* TradeTooltip = nullptr;

    UFUNCTION() void HandleTradeInvSlotClicked(int32 SlotIndex);
    UFUNCTION() void HandleTradeInvSlotHovered(int32 SlotIndex, bool bIsHovered);
    UFUNCTION() void HandleMyOfferSlotHovered(int32 SlotIndex, bool bIsHovered);
    UFUNCTION() void HandleTheirOfferSlotHovered(int32 SlotIndex, bool bIsHovered);
    void ShowOfferTooltip(const TArray<UVendorSlotWidget*>& Slots, int32 SlotIndex, bool bIsHovered);
    void EnsureTradeTooltip();

    FTradeStateData CurrentState;
    FCharacterInventoryStruct CachedInventory;
    // Accumulated offer on client side before sending
    TArray<FTradeOfferItem> PendingOffer;
    int32 PendingGold = 0;

    // One-shot trade hint toasts (distance / offer-changed / cancel reason).
    void ShowTradeHint(const FString& Title, const FString& Body);

    // Set while WE edit the offer (DispatchAddItem): the next state echo that
    // unconfirms us is ours, not the partner changing the deal.
    bool bOwnOfferPending = false;
    // Distance warning: one-shot per excursion beyond trade range.
    bool bWarnedTooFar = false;
    double LastDistCheckTime = 0.0;

    bool      bDragging = false;
    FVector2D DragOffset = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;

    UFUNCTION()
    void HandleLocaleChanged(const FString& NewLocale);
};
