#include "UI/VendorSlotWidget.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/ItemManager.h"
#include "Engine/AssetManager.h"
#include "Components/SizeBox.h"

// ---------------------------------------------------------------------------
// NativeConstruct / NativePreConstruct
// ---------------------------------------------------------------------------

void UVendorSlotWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    SetSlotSide(SlotSide);
}

void UVendorSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetSlotSide(SlotSide);

    if (SlotButton)
    {
        SlotButton->OnClicked.AddDynamic(this, &UVendorSlotWidget::HandleSlotButtonClicked);
    }
    if (RemoveButton)
    {
        RemoveButton->OnClicked.AddDynamic(this, &UVendorSlotWidget::HandleRemoveButtonClicked);
        RemoveButton->SetVisibility(ESlateVisibility::Collapsed);
    }
    // Do NOT force-hide optional widgets here — NativeConstruct fires again after
    // AddChild(), which would overwrite state already set by SetupAs*() calls.
    // RefreshVisuals() manages all visibility unconditionally.
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void UVendorSlotWidget::SetupAsShopSlot(int32 InSlotIndex, const FVendorShopItemData& InItem, bool bIsInCart, bool bIsFullyInCart, int32 InCartQuantity)
{
    Mode         = EVendorSlotMode::ShopItem;
    SlotIndex    = InSlotIndex;
    ShopItemData = InItem;
    bInCart      = bIsInCart;
    CartQuantity = InCartQuantity;
    // Disabled when: out of stock OR all available stock already in cart
    bIsDisabled  = (InItem.stockCurrent == 0) || bIsFullyInCart;
    RefreshVisuals();
}

void UVendorSlotWidget::SetupAsInvSlot(int32 InSlotIndex, const FInventoryItemStruct& InItem, bool bIsInCart, bool bIsFullyInCart, int32 InCartQuantity)
{
    Mode         = EVendorSlotMode::InvItem;
    SlotIndex    = InSlotIndex;
    InvItemData  = InItem;
    bInCart      = bIsInCart;
    CartQuantity = InCartQuantity;
    // Disabled when: non-tradable, quest item, or full quantity already in cart
    bIsDisabled  = (!InItem.isTradable || InItem.isQuestItem) || bIsFullyInCart;
    RefreshVisuals();
}

void UVendorSlotWidget::SetupAsCartSlot(int32 InSlotIndex, const FVendorCartEntry& InEntry)
{
    Mode          = EVendorSlotMode::CartEntry;
    SlotIndex     = InSlotIndex;
    CartEntryData = InEntry;
    CartQuantity  = InEntry.quantity;
    bInCart       = true;
    bIsDisabled   = false;
    RefreshVisuals();
}

void UVendorSlotWidget::SetInCart(bool bNewInCart)
{
    bInCart = bNewInCart;
    if (InCartImage)
        InCartImage->SetVisibility(bInCart ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UVendorSlotWidget::SetAffordability(EVendorSlotAffordability InAffordability)
{
    if (!AffordabilityBorder) return;

    switch (InAffordability)
    {
        case EVendorSlotAffordability::NotEnoughGold:
            AffordabilityBorder->SetColorAndOpacity(ColorNotEnoughGold);
            AffordabilityBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
            break;
        case EVendorSlotAffordability::LevelTooLow:
            AffordabilityBorder->SetColorAndOpacity(ColorLevelTooLow);
            AffordabilityBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
            break;
        default: // Affordable
            AffordabilityBorder->SetVisibility(ESlateVisibility::Collapsed);
            break;
    }
}

// ---------------------------------------------------------------------------
// Tooltip data accessor
// ---------------------------------------------------------------------------

FInventoryItemStruct UVendorSlotWidget::GetTooltipData() const
{
    switch (Mode)
    {
        case EVendorSlotMode::ShopItem:  return MakePreviewFromShopItem(ShopItemData);
        case EVendorSlotMode::InvItem:   return InvItemData;
        case EVendorSlotMode::CartEntry:
        {
            // Build minimal preview from cart entry
            FInventoryItemStruct Preview;
            Preview.itemId           = CartEntryData.itemId;
            Preview.id               = CartEntryData.inventoryItemId;
            Preview.slug             = CartEntryData.slug;
            Preview.quantity         = CartEntryData.quantity;
            Preview.priceBuy = CartEntryData.pricePerUnit;
            return Preview;
        }
        default: return FInventoryItemStruct();
    }
}

// ---------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------

void UVendorSlotWidget::RefreshVisuals()
{
    // Icon
    const FString Slug = (Mode == EVendorSlotMode::ShopItem)  ? ShopItemData.slug
                       : (Mode == EVendorSlotMode::InvItem)   ? InvItemData.slug
                       :                                        CartEntryData.slug;
    LoadIconBySlug(Slug);

    // Disabled overlay (out of stock, non-tradable, quest)
    if (DisabledOverlay)
        DisabledOverlay->SetVisibility(bIsDisabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    if (SlotButton)
        SlotButton->SetIsEnabled(!bIsDisabled);

    // In-cart checkmark
    if (InCartImage)
        InCartImage->SetVisibility(bInCart ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    // Remove button — only visible in cart mode
    if (RemoveButton)
        RemoveButton->SetVisibility(Mode == EVendorSlotMode::CartEntry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // Overlay badge text:
    //   ShopItem  -> stock count ("∞" for unlimited, "5" for finite)
    //   InvItem   -> player's owned quantity ("x10")
    //   CartEntry -> quantity in cart ("x2") — same as CartQtyText but slot is cart-only
    if (OverlayBadgeText)
    {
        FString BadgeStr;
        if (Mode == EVendorSlotMode::ShopItem)
        {
            // Unlimited stock: always "∞" regardless of cart qty
            // Finite stock: show remaining available = stockCurrent - cartQuantity
            if (ShopItemData.stockCurrent == -1)
            {
                BadgeStr = TEXT("\u221E");
            }
            else
            {
                const int32 Remaining = FMath::Max(0, ShopItemData.stockCurrent - CartQuantity);
                BadgeStr = FString::Printf(TEXT("%d"), Remaining);
            }
        }
        else if (Mode == EVendorSlotMode::InvItem)
        {
            // Show remaining available to sell = owned quantity - cart quantity
            const int32 Remaining = FMath::Max(0, InvItemData.quantity - CartQuantity);
            BadgeStr = FString::Printf(TEXT("%d"), Remaining);
        }
        // CartEntry: quantity is shown via CartQtyText, not OverlayBadgeText
        // Leave BadgeStr empty so OverlayBadgeText is collapsed for cart slots

        if (!BadgeStr.IsEmpty())
        {
            OverlayBadgeText->SetText(FText::FromString(BadgeStr));
            OverlayBadgeText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            OverlayBadgeText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Cart quantity badge: CartEntry only — hidden for ShopItem / InvItem slots
    if (CartQtyText)
    {
        if (Mode == EVendorSlotMode::CartEntry)
        {
            CartQtyText->SetText(FText::FromString(FString::Printf(TEXT("%d"), CartEntryData.quantity)));
            CartQtyText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            CartQtyText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Price text:
    //   ShopItem  -> buy price per unit  ("150 g")
    //   InvItem   -> sell price per unit ("50 g")
    //   CartEntry -> total line price    ("300 g")
    if (PriceText)
    {
        FString PriceStr;
        if (Mode == EVendorSlotMode::ShopItem)
            PriceStr = FString::Printf(TEXT("%d g"), ShopItemData.priceBuy);
        else if (Mode == EVendorSlotMode::InvItem)
        PriceStr = FString::Printf(TEXT("%d g"), InvItemData.priceSell);
        else
            PriceStr = FString::Printf(TEXT("%d g"), CartEntryData.pricePerUnit * CartEntryData.quantity);

        PriceText->SetText(FText::FromString(PriceStr));
        PriceText->SetVisibility(ESlateVisibility::Visible);
    }
}

// ---------------------------------------------------------------------------
// Icon loading (same pattern as InventorySlotWidget)
// ---------------------------------------------------------------------------

void UVendorSlotWidget::LoadIconBySlug(const FString& Slug)
{
    if (!ItemIcon || Slug.IsEmpty()) return;

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr);
    if (!GI) return;

    UItemManager* ItemMgr = GI->GetItemManager();
    if (!ItemMgr) return;

    FItemVisualData VisualData = ItemMgr->GetItemVisualDataBySlug(Slug);
    if (VisualData.Icon.IsNull())
    {
        ItemIcon->SetBrushFromTexture(nullptr);
        return;
    }

    if (UTexture2D* Already = VisualData.Icon.Get())
    {
        SetIconTexture(Already);
        return;
    }

    TWeakObjectPtr<UVendorSlotWidget> WeakThis = this;
    TSoftObjectPtr<UTexture2D> SoftIcon = VisualData.Icon;
    AsyncLoad(SoftIcon.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([WeakThis, SoftIcon]()
        {
            if (WeakThis.IsValid())
                if (UTexture2D* Tex = SoftIcon.Get())
                    WeakThis->SetIconTexture(Tex);
        })
    );
}

void UVendorSlotWidget::SetIconTexture(UTexture2D* Texture)
{
    if (ItemIcon && Texture)
    {
        ItemIcon->SetBrushFromTexture(Texture);
        ItemIcon->SetColorAndOpacity(FLinearColor::White);
        ItemIcon->SetVisibility(ESlateVisibility::Visible);
    }
}

void UVendorSlotWidget::AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback)
{
    if (AssetPath.IsValid())
    {
        StreamableHandle = UAssetManager::Get().GetStreamableManager()
            .RequestAsyncLoad(AssetPath, Callback);
    }
}

// ---------------------------------------------------------------------------
// Conversion helper
// ---------------------------------------------------------------------------

FInventoryItemStruct UVendorSlotWidget::MakePreviewFromShopItem(const FVendorShopItemData& V)
{
    FInventoryItemStruct Out;
    Out.itemId       = V.itemId;
    Out.slug         = V.slug;
    Out.itemTypeSlug = V.itemTypeSlug;
    Out.raritySlug   = V.raritySlug;
    Out.priceBuy     = V.priceBuy;
    Out.priceSell    = V.priceSell;
    Out.isTradable   = V.isTradable;
    Out.isDurable    = V.isDurable;
    Out.quantity     = 1;
    return Out;
}

// ---------------------------------------------------------------------------
// Slot size
// ---------------------------------------------------------------------------

void UVendorSlotWidget::SetSlotSide(float InSide)
{
    SlotSide = InSide;
    if (SlotSizeBox)
    {
        SlotSizeBox->SetWidthOverride(SlotSide);
        SlotSizeBox->SetHeightOverride(SlotSide);
    }
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void UVendorSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    // NativeOnMouseMove will decide whether to show the tooltip
    bRemoveButtonHovered = false;
    OnVendorSlotHovered.Broadcast(SlotIndex, true);
}

void UVendorSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    bRemoveButtonHovered = false;
    OnVendorSlotHovered.Broadcast(SlotIndex, false);
}

FReply UVendorSlotWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Check whether the cursor is currently over the RemoveButton.
    // If so, suppress the tooltip; restore it when the cursor moves away.
    if (RemoveButton && RemoveButton->GetVisibility() == ESlateVisibility::Visible)
    {
        const FGeometry BtnGeo = RemoveButton->GetCachedGeometry();
        const bool bOverBtn   = BtnGeo.IsUnderLocation(InMouseEvent.GetScreenSpacePosition());

        if (bOverBtn && !bRemoveButtonHovered)
        {
            bRemoveButtonHovered = true;
            OnVendorSlotHovered.Broadcast(SlotIndex, false); // hide tooltip
        }
        else if (!bOverBtn && bRemoveButtonHovered)
        {
            bRemoveButtonHovered = false;
            OnVendorSlotHovered.Broadcast(SlotIndex, true);  // restore tooltip
        }
    }

    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// ---------------------------------------------------------------------------
// Button handlers
// ---------------------------------------------------------------------------

void UVendorSlotWidget::HandleSlotButtonClicked()
{
    if (!bIsDisabled)
        OnVendorSlotClicked.Broadcast(SlotIndex);
}

void UVendorSlotWidget::HandleRemoveButtonClicked()
{
    OnRemoveFromCart.Broadcast(SlotIndex);
}

