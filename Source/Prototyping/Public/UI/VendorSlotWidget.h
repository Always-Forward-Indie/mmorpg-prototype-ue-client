#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Data/DataStructs.h"
#include "Engine/StreamableManager.h"
#include "VendorSlotWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVendorSlotClicked,       int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVendorSlotHovered,      int32, SlotIndex, bool, bIsHovered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVendorCartSlotClicked,   int32, SlotIndex);

UENUM(BlueprintType)
enum class EVendorSlotAffordability : uint8
{
    Affordable    UMETA(DisplayName = "Affordable"),          // can buy right now
    NotEnoughGold UMETA(DisplayName = "Not Enough Gold"),     // price > current gold
    LevelTooLow   UMETA(DisplayName = "Level Too Low"),       // level requirement not met
};

UENUM(BlueprintType)
enum class EVendorSlotMode : uint8
{
    ShopItem  UMETA(DisplayName = "Shop Item (Buy)"),
    InvItem   UMETA(DisplayName = "Inventory Item (Sell)"),
    CartEntry UMETA(DisplayName = "Cart Entry")
};

/**
 * UVendorSlotWidget
 *
 * Reusable slot widget used in:
 *   - Vendor shop grid   (Mode = ShopItem,  data from FVendorShopItemData)
 *   - Inventory grid     (Mode = InvItem,   data from FInventoryItemStruct)
 *   - Cart grid          (Mode = CartEntry, data from FVendorCartEntry)
 *
 * Blueprint must bind:
 *   SlotSizeBox       USizeBox
 *   SlotButton        UButton
 *   ItemIcon          UImage
 *   OverlayBadgeText  UTextBlock   stock count (ShopItem: "\u221E"/"5") or inv qty (InvItem: "x10")  (BindWidgetOptional)
 *   CartQtyText       UTextBlock   entry quantity in cart ("x2") — CartEntry only              (BindWidgetOptional)
 *   PriceText         UTextBlock   price label below slot: buy/sell price per unit, cart line total  (BindWidgetOptional)
 *   InCartImage       UImage       green checkmark overlay when item is in cart                      (BindWidgetOptional)
 *   DisabledOverlay   UImage       grey darkening overlay when slot is disabled                      (BindWidgetOptional)
 *   RemoveButton      UButton      [X] button shown only on CartEntry slots                          (BindWidgetOptional)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UVendorSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // -----------------------------------------------------------------------
    // Setup — call one of these depending on the mode
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Vendor Slot")
    void SetupAsShopSlot(int32 InSlotIndex, const FVendorShopItemData& InItem, bool bInCart, bool bIsFullyInCart = false, int32 InCartQuantity = 0);

    UFUNCTION(BlueprintCallable, Category = "Vendor Slot")
    void SetupAsInvSlot(int32 InSlotIndex, const FInventoryItemStruct& InItem, bool bInCart, bool bIsFullyInCart = false, int32 InCartQuantity = 0);

    UFUNCTION(BlueprintCallable, Category = "Vendor Slot")
    void SetupAsCartSlot(int32 InSlotIndex, const FVendorCartEntry& InEntry);

    // Mark / unmark "in cart" state without full rebuild
    UFUNCTION(BlueprintCallable, Category = "Vendor Slot")
    void SetInCart(bool bNewInCart);

    // Set affordability highlight (ShopItem / CartEntry only; ignored for InvItem)
    UFUNCTION(BlueprintCallable, Category = "Vendor Slot")
    void SetAffordability(EVendorSlotAffordability InAffordability);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vendor Slot")
    int32 GetSlotIndex() const { return SlotIndex; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vendor Slot")
    EVendorSlotMode GetMode() const { return Mode; }

    // Returns FInventoryItemStruct preview — works for all modes via conversion
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vendor Slot")
    FInventoryItemStruct GetTooltipData() const;

    // Raw data accessors — prefer these for typed tooltip population
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vendor Slot")
    const FVendorShopItemData& GetShopItemData() const { return ShopItemData; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vendor Slot")
    const FInventoryItemStruct& GetInvItemData() const { return InvItemData; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Vendor Slot")
    const FVendorCartEntry& GetCartEntryData() const { return CartEntryData; }

    // Size override (matches InventorySlotWidget pattern)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Slot", meta = (ClampMin = "8"))
    float SlotSide = 64.f;

    UFUNCTION(BlueprintCallable, Category = "Vendor Slot")
    void SetSlotSide(float InSide);

    // -----------------------------------------------------------------------
    // Events
    // -----------------------------------------------------------------------

    UPROPERTY(BlueprintAssignable, Category = "Vendor Slot Events")
    FOnVendorSlotClicked OnVendorSlotClicked;

    UPROPERTY(BlueprintAssignable, Category = "Vendor Slot Events")
    FOnVendorSlotHovered OnVendorSlotHovered;

    // Fired only when Mode == CartEntry and RemoveButton is clicked
    UPROPERTY(BlueprintAssignable, Category = "Vendor Slot Events")
    FOnVendorCartSlotClicked OnRemoveFromCart;

protected:
    virtual void NativeConstruct() override;
    virtual void NativePreConstruct() override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // -----------------------------------------------------------------------
    // Blueprint-bound widgets
    // -----------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    USizeBox* SlotSizeBox = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UButton* SlotButton = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* ItemIcon = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* OverlayBadgeText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* PriceText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UImage* InCartImage = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UImage* DisabledOverlay = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* RemoveButton = nullptr;

    // Shows the quantity of this entry in the cart ("x2").
    // Visible only on CartEntry slots; always hidden for ShopItem / InvItem.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* CartQtyText = nullptr;

    // Tinted border overlay driven by EVendorSlotAffordability.
    // Invisible when Affordable; red-tinted when gold is insufficient; yellow-tinted when level is too low.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UImage* AffordabilityBorder = nullptr;

    // Colors applied to AffordabilityBorder (can be overridden in Blueprint defaults)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Slot|Affordability")
    FLinearColor ColorNotEnoughGold = FLinearColor(0.9f, 0.15f, 0.15f, 0.55f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Slot|Affordability")
    FLinearColor ColorLevelTooLow   = FLinearColor(0.95f, 0.75f, 0.05f, 0.55f);

private:
    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    EVendorSlotMode Mode = EVendorSlotMode::ShopItem;
    int32 SlotIndex      = -1;
    bool  bInCart        = false;
    bool  bIsDisabled    = false;
    int32 CartQuantity   = 0;
    bool  bRemoveButtonHovered = false;

    // Raw data per mode (only one is valid at a time)
    FVendorShopItemData   ShopItemData;
    FInventoryItemStruct  InvItemData;
    FVendorCartEntry      CartEntryData;

    // -----------------------------------------------------------------------
    // Internals
    // -----------------------------------------------------------------------

    void RefreshVisuals();
    void LoadIconBySlug(const FString& Slug);
    void SetIconTexture(UTexture2D* Texture);
    void AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback);

    // Convert FVendorShopItemData to FInventoryItemStruct for tooltip compatibility
    static FInventoryItemStruct MakePreviewFromShopItem(const FVendorShopItemData& V);

    TSharedPtr<FStreamableHandle> StreamableHandle;

    UFUNCTION()
    void HandleSlotButtonClicked();

    UFUNCTION()
    void HandleRemoveButtonClicked();
};
