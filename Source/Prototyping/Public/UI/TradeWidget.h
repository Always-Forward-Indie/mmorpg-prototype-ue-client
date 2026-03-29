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
#include "TradeWidget.generated.h"

class UTradeManager;
class UInventoryManager;
class UTradeInvItemRowBinding;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeWidgetVisibilityChanged, bool, bIsVisible);

/**
 * TradeWidget
 *
 * Full P2P trade session UI:
 *   Left panel  — my offer (items + gold I'm giving)
 *   Right panel — their offer (items + gold they're giving)
 *   Confirm / Cancel buttons
 *   Pending invite banner — Accept / Decline
 *
 * Blueprint subclass must bind:
 *   My_Items_Box       UScrollBox   — my offered items
 *   Their_Items_Box    UScrollBox   — partner's offered items
 *   Inv_Items_Box      UScrollBox   — my inventory (items to add to offer)
 *   My_Gold_Text       UTextBlock   — gold I'm offering
 *   Their_Gold_Text    UTextBlock   — gold partner is offering
 *   My_Confirm_Text    UTextBlock   — "Confirmed" / "Pending"
 *   Their_Confirm_Text UTextBlock   — "Confirmed" / "Pending"
 *   Gold_Input         UEditableTextBox — type gold amount (BindWidgetOptional)
 *   Confirm_Btn        UButton
 *   Cancel_Btn         UButton
 *   Close_Button       UButton      (BindWidgetOptional)
 *
 *   Invite_Banner      UWidget      (BindWidgetOptional) — shown on tradeInvite
 *   Invite_Name_Text   UTextBlock   (BindWidgetOptional)
 *   Invite_Accept_Btn  UButton      (BindWidgetOptional)
 *   Invite_Decline_Btn UButton      (BindWidgetOptional)
 *
 * Each inv-row widget (InvRowClass) needs:
 *   Row_Name_Text      UTextBlock
 *   Row_Add_Btn        UButton
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade UI")
    TSubclassOf<UUserWidget> InvRowClass;

    UPROPERTY(BlueprintAssignable, Category = "Trade UI Events")
    FOnTradeWidgetVisibilityChanged OnTradeVisibilityChanged;

    // Called by row binding helpers
    void DispatchAddItem(int32 InventoryItemId, int32 ItemId);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    UFUNCTION() void HandleTradeInviteReceived(const FTradeInviteData& Invite);
    UFUNCTION() void HandleTradeStateUpdated(const FTradeStateData& State);
    UFUNCTION() void HandleTradeDeclined(const FTradeDeclinedData& Data);
    UFUNCTION() void HandleTradeCancelled(const FTradeCancelledData& Data);
    UFUNCTION() void HandleTradeCompleted(const FTradeCompleteData& Data);
    UFUNCTION() void HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory);

    UFUNCTION() void HandleConfirmClicked();
    UFUNCTION() void HandleCancelClicked();
    UFUNCTION() void HandleCloseButtonClicked();
    UFUNCTION() void HandleInviteAcceptClicked();
    UFUNCTION() void HandleInviteDeclineClicked();

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
    void RebuildItemsList(UScrollBox* Box, const TArray<FInventoryItemStruct>& Items);
    void RebuildItemsList(UScrollBox* Box, const TArray<FTradeOfferItem>& Items);
    void RebuildInventoryList();

    UPROPERTY() UTradeManager*    TradeManager    = nullptr;
    UPROPERTY() UInventoryManager* InventoryManager = nullptr;

    // Inv row binding helpers
    UPROPERTY() TArray<UTradeInvItemRowBinding*> InvRowBindings;

    FTradeStateData CurrentState;
    FCharacterInventoryStruct CachedInventory;
    // Accumulated offer on client side before sending
    TArray<FTradeOfferItem> PendingOffer;
    int32 PendingGold = 0;

    bool      bDragging = false;
    FVector2D DragOffset = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
};

// ---------------------------------------------------------------------------
// Per-row binding helper
// ---------------------------------------------------------------------------

UCLASS()
class PROTOTYPING_API UTradeInvItemRowBinding : public UObject
{
    GENERATED_BODY()

public:
    void Setup(UTradeWidget* InWidget, int32 InInventoryItemId, int32 InItemId);

    UFUNCTION()
    void HandleClicked();

private:
    UPROPERTY() UTradeWidget* Widget = nullptr;
    int32 InventoryItemId = 0;
    int32 ItemId          = 0;
};
