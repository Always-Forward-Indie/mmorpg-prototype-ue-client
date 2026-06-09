#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/WrapBox.h"
#include "Data/DataStructs.h"
#include "UI/VendorSlotWidget.h"
#include "UI/QuantityPopupWidget.h"
#include "UI/VendorTooltipWidget.h"
#include "VendorShopWidget.generated.h"

class UVendorManager;
class UInventoryManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVendorShopVisibilityChanged, bool, bIsVisible);

UENUM(BlueprintType)
enum class EVendorTab : uint8
{
    Buy  UMETA(DisplayName = "Buy"),
    Sell UMETA(DisplayName = "Sell")
};

/**
 * UVendorShopWidget
 *
 * Vendor shop with Buy/Sell tabs, slot grids, cart and quantity popup.
 *
 * Blueprint subclass must bind:
 *   Buy_Items_Box        UWrapBox    vendor catalogue slots
 *   Sell_Items_Box       UWrapBox    player inventory slots (tradable only)
 *   Buy_Cart_Box         UWrapBox    buy cart slots
 *   Sell_Cart_Box        UWrapBox    sell cart slots
 *   Buy_Panel            UWidget     panel shown on Buy tab
 *   Sell_Panel           UWidget     panel shown on Sell tab
 *   Tab_Buy_Button       UButton
 *   Tab_Sell_Button      UButton
 *   NPC_Name_Text        UTextBlock
 *   Buy_Total_Text       UTextBlock  buy cart total price   (BindWidgetOptional)
 *   Sell_Total_Text      UTextBlock  sell cart total gold   (BindWidgetOptional)
 *   Status_Text          UTextBlock  result messages        (BindWidgetOptional)
 *   Close_Button         UButton                            (BindWidgetOptional)
 *   Buy_Confirm_Button   UButton                            (BindWidgetOptional)
 *   Sell_Confirm_Button  UButton                            (BindWidgetOptional)
 *   Buy_Clear_Button     UButton                            (BindWidgetOptional)
 *   Sell_Clear_Button    UButton                            (BindWidgetOptional)
 *   Player_Gold_Text     UTextBlock  player's current gold  (BindWidgetOptional)
 *   DragHandle           UWidget                            (BindWidgetOptional)
 *
 * Set in Blueprint:
 *   VendorSlotClass     TSubclassOf<UVendorSlotWidget>
 *   QuantityPopupClass  TSubclassOf<UQuantityPopupWidget>
 *   TooltipWidget       UItemTooltipWidget* (optional, for hover tooltips)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UVendorShopWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Vendor UI")
    void BindToManagers(UVendorManager* InVendorManager, UInventoryManager* InInventoryManager);

    UFUNCTION(BlueprintCallable, Category = "Vendor UI")
    void OpenShop(EVendorTab DefaultTab = EVendorTab::Buy);

    UFUNCTION(BlueprintCallable, Category = "Vendor UI")
    void CloseShop();

    /** Returns the NPC id whose items are currently shown (0 if no shop loaded). */
    int32 GetActiveNpcId() const { return CachedShop.npcId; }

    UFUNCTION(BlueprintCallable, Category = "Vendor UI")
    void SwitchToTab(EVendorTab Tab);

    UFUNCTION(BlueprintCallable, Category = "Vendor UI")
    void RefreshShopDisplay();

    UFUNCTION(BlueprintCallable, Category = "Vendor UI")
    void RefreshInventoryDisplay();

    UFUNCTION(BlueprintCallable, Category = "Vendor UI")
    void RefreshBuyCartDisplay();

    UFUNCTION(BlueprintCallable, Category = "Vendor UI")
    void RefreshSellCartDisplay();

    // Widget classes -- assign in Blueprint
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor UI")
    TSubclassOf<UVendorSlotWidget> VendorSlotClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor UI")
    TSubclassOf<UQuantityPopupWidget> QuantityPopupClass;

    // Tooltip widget class -- assign in Blueprint (like InventoryWidget pattern)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor UI")
    TSubclassOf<UVendorTooltipWidget> VendorTooltipWidgetClass;

    UPROPERTY(BlueprintAssignable, Category = "Vendor UI Events")
    FOnVendorShopVisibilityChanged OnVendorShopVisibilityChanged;

protected:
virtual void NativeConstruct() override;
virtual void NativeDestruct() override;
virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    // Delegate handlers from managers
    UFUNCTION() void HandleVendorShopOpened(const FVendorShopData& ShopData);
    UFUNCTION() void HandleBuyItemResult(const FBuyItemResultData& Result);
    UFUNCTION() void HandleSellItemResult(const FSellItemResultData& Result);
    UFUNCTION() void HandleBuyItemBatchResult(const FBuyItemBatchResultData& Result);
    UFUNCTION() void HandleSellItemBatchResult(const FSellItemBatchResultData& Result);
    UFUNCTION() void HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory);
    UFUNCTION() void HandleCloseButtonClicked();
    UFUNCTION() void HandleTabBuyClicked();
    UFUNCTION() void HandleTabSellClicked();
    UFUNCTION() void HandleConfirmBuyCart();
    UFUNCTION() void HandleConfirmSellCart();
    UFUNCTION() void HandleClearBuyCart();
    UFUNCTION() void HandleClearSellCart();

    // Slot event handlers
    UFUNCTION() void HandleShopSlotClicked(int32 SlotIndex);
    UFUNCTION() void HandleShopSlotHovered(int32 SlotIndex, bool bIsHovered);
    UFUNCTION() void HandleInvSlotClicked(int32 SlotIndex);
    UFUNCTION() void HandleInvSlotHovered(int32 SlotIndex, bool bIsHovered);
    UFUNCTION() void HandleBuyCartSlotClicked(int32 SlotIndex);
    UFUNCTION() void HandleSellCartSlotClicked(int32 SlotIndex);
    UFUNCTION() void HandleBuyCartSlotRemove(int32 SlotIndex);
    UFUNCTION() void HandleSellCartSlotRemove(int32 SlotIndex);
    UFUNCTION() void HandleBuyCartSlotHovered(int32 SlotIndex, bool bIsHovered);
    UFUNCTION() void HandleSellCartSlotHovered(int32 SlotIndex, bool bIsHovered);

    // Quantity popup handlers
    UFUNCTION() void HandleBuyQuantityConfirmed(int32 SlotIndex, int32 Quantity);
    UFUNCTION() void HandleBuyQuantityRemove(int32 SlotIndex);
    UFUNCTION() void HandleSellQuantityConfirmed(int32 SlotIndex, int32 Quantity);
    UFUNCTION() void HandleSellQuantityRemove(int32 SlotIndex);
    // Cart-slot popup handlers (SlotIndex is index inside the cart array)
    UFUNCTION() void HandleBuyCartQuantityConfirmed(int32 CartIndex, int32 Quantity);
    UFUNCTION() void HandleBuyCartQuantityRemove(int32 CartIndex);
    UFUNCTION() void HandleSellCartQuantityConfirmed(int32 CartIndex, int32 Quantity);
    UFUNCTION() void HandleSellCartQuantityRemove(int32 CartIndex);

    // ---------------------------------------------------------------------------
    // Blueprint-bound widgets
    // ---------------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UWrapBox*   Buy_Items_Box       = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UWrapBox*   Sell_Items_Box      = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UWrapBox*   Buy_Cart_Box        = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UWrapBox*   Sell_Cart_Box       = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UWidget*    Buy_Panel           = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UWidget*    Sell_Panel          = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UButton*    Tab_Buy_Button      = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UButton*    Tab_Sell_Button     = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UTextBlock* NPC_Name_Text       = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* Buy_Total_Text      = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* Sell_Total_Text     = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* Status_Text         = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*    Close_Button        = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*    Buy_Confirm_Button  = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*    Sell_Confirm_Button = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*    Buy_Clear_Button    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*    Sell_Clear_Button   = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget*    DragHandle          = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* Player_Gold_Text    = nullptr;

private:
UPROPERTY() UVendorManager*    VendorManager    = nullptr;
UPROPERTY() UInventoryManager* InventoryManager = nullptr;

// Active quantity popup instance (one shared, reused)
UPROPERTY() UQuantityPopupWidget* ActivePopup = nullptr;

// Tooltip instance -- created in NativeConstruct, lives at high z-order on viewport
UPROPERTY() UVendorTooltipWidget* VendorTooltipWidget = nullptr;

    // Cached data
    FVendorShopData           CachedShop;
    FCharacterInventoryStruct CachedInventory;
    EVendorTab                ActiveTab = EVendorTab::Buy;

    // Carts
    TArray<FVendorCartEntry> BuyCart;
    TArray<FVendorCartEntry> SellCart;

    // Slot widget arrays (kept for index lookup)
    UPROPERTY() TArray<UVendorSlotWidget*> ShopSlots;
    UPROPERTY() TArray<UVendorSlotWidget*> InvSlots;
    UPROPERTY() TArray<UVendorSlotWidget*> BuyCartSlots;
    UPROPERTY() TArray<UVendorSlotWidget*> SellCartSlots;

    // Helpers
    void ShowStatus(const FString& Msg);
    FText GetVendorErrorText(const FString& ErrorCode) const;
    void UpdateBuyTotalText();
    void UpdateSellTotalText();
    void UpdatePlayerGoldText();
    void EnsurePopup();
    bool IsItemInBuyCart(int32 ItemId, int32& OutCartIndex) const;
    bool IsItemInSellCart(int32 InventoryItemId, int32& OutCartIndex) const;

    // Returns current player level from PlayerStatsManager; 0 if not available
    int32 GetPlayerLevel() const;

    // Compute affordability for a shop slot: checks gold and level requirement
    EVendorSlotAffordability GetShopItemAffordability(const FVendorShopItemData& Item, int32 TotalCartCost) const;
    // Compute affordability for a buy cart entry
    EVendorSlotAffordability GetCartEntryAffordability(int32 TotalBuyCartCost) const;

    int32   GetCharacterId() const;
    FVector GetPlayerPosition() const;

    // Returns a localised display name for the item slug, falls back to slug itself
    FString GetLocalizedItemName(const FString& ItemSlug) const;

    // Drag
    bool      bDragging               = false;
    FVector2D DragOffset              = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
};

