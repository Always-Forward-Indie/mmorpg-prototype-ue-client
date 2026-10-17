#include "UI/VendorShopWidget.h"
#include "UI/VendorShopWidget.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "UI/VendorTooltipWidget.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WrapBox.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "EngineUtils.h"

void UVendorShopWidget::HandleShopSlotHovered(int32 SlotIndex, bool bIsHovered)
{
    if (!VendorTooltipWidget) return;
    if (bIsHovered && CachedShop.items.IsValidIndex(SlotIndex))
    {
        VendorTooltipWidget->SetDataFromShopItem(CachedShop.items[SlotIndex]);
        VendorTooltipWidget->ShowTooltip();
    }
    else VendorTooltipWidget->HideTooltip();
}

void UVendorShopWidget::HandleInvSlotHovered(int32 SlotIndex, bool bIsHovered)
{
    if (!VendorTooltipWidget) return;
    if (bIsHovered && InvSlots.IsValidIndex(SlotIndex))
    {
        VendorTooltipWidget->SetDataFromInventoryItem(InvSlots[SlotIndex]->GetInvItemData());
        VendorTooltipWidget->ShowTooltip();
    }
    else VendorTooltipWidget->HideTooltip();
}

void UVendorShopWidget::HandleShopSlotClicked(int32 SlotIndex)
{
    if (!CachedShop.items.IsValidIndex(SlotIndex)) return;
    const FVendorShopItemData& Item = CachedShop.items[SlotIndex];

    EnsurePopup();
    ActivePopup->OnQuantityConfirmed.Clear();
    ActivePopup->OnQuantityRemove   .Clear();
    ActivePopup->OnQuantityConfirmed.AddDynamic(this, &UVendorShopWidget::HandleBuyQuantityConfirmed);
    ActivePopup->OnQuantityRemove   .AddDynamic(this, &UVendorShopWidget::HandleBuyQuantityRemove);

    // stockCurrent == -1 means unlimited supply, use stackMax as the cap
    // stockCurrent >= 0 means finite stock � that is the hard ceiling
    const int32 StockLimit = (Item.stockCurrent == -1) ? Item.stackMax : Item.stockCurrent;

    int32 CartIdx = -1;
    const int32 AlreadyInCart = IsItemInBuyCart(Item.itemId, CartIdx) ? BuyCart[CartIdx].quantity : 0;
    // PopupMax = total stock available (popup starts at current cart qty, max = StockLimit)
    const int32 PopupMax = StockLimit;

    if (IsItemInBuyCart(Item.itemId, CartIdx))
        ActivePopup->OpenForUpdate(SlotIndex, GetLocalizedItemName(Item.slug), Item.priceBuy, PopupMax, BuyCart[CartIdx].quantity);
    else
        ActivePopup->OpenForAdd(SlotIndex, GetLocalizedItemName(Item.slug), Item.priceBuy, PopupMax);
}

void UVendorShopWidget::HandleInvSlotClicked(int32 SlotIndex)
{
    if (!InvSlots.IsValidIndex(SlotIndex)) return;
    const FInventoryItemStruct Item = InvSlots[SlotIndex]->GetTooltipData();

    EnsurePopup();
    ActivePopup->OnQuantityConfirmed.Clear();
    ActivePopup->OnQuantityRemove   .Clear();
    ActivePopup->OnQuantityConfirmed.AddDynamic(this, &UVendorShopWidget::HandleSellQuantityConfirmed);
    ActivePopup->OnQuantityRemove   .AddDynamic(this, &UVendorShopWidget::HandleSellQuantityRemove);

    int32 CartIdx = -1;
    // Popup max = total item quantity (player picks up to that; already-in-cart shown as current)
    const int32 PopupMax = Item.quantity;

    if (IsItemInSellCart(Item.id, CartIdx))
        ActivePopup->OpenForUpdate(SlotIndex, GetLocalizedItemName(Item.slug), Item.priceSell, PopupMax, SellCart[CartIdx].quantity);
    else
        ActivePopup->OpenForAdd(SlotIndex, GetLocalizedItemName(Item.slug), Item.priceSell, PopupMax);
}

void UVendorShopWidget::HandleBuyCartSlotClicked(int32 SlotIndex)
{
    if (!BuyCart.IsValidIndex(SlotIndex)) return;
    const FVendorCartEntry& Entry = BuyCart[SlotIndex];
    EnsurePopup();
    ActivePopup->OnQuantityConfirmed.Clear();
    ActivePopup->OnQuantityRemove   .Clear();
    // Use negative-offset convention: pass -(SlotIndex+1) to distinguish cart clicks from shop clicks
    ActivePopup->OnQuantityConfirmed.AddDynamic(this, &UVendorShopWidget::HandleBuyCartQuantityConfirmed);
    ActivePopup->OnQuantityRemove   .AddDynamic(this, &UVendorShopWidget::HandleBuyCartQuantityRemove);
    ActivePopup->OpenForUpdate(SlotIndex, GetLocalizedItemName(Entry.slug), Entry.pricePerUnit, Entry.maxQuantity, Entry.quantity);
}

void UVendorShopWidget::HandleSellCartSlotClicked(int32 SlotIndex)
{
    if (!SellCart.IsValidIndex(SlotIndex)) return;
    const FVendorCartEntry& Entry = SellCart[SlotIndex];
    EnsurePopup();
    ActivePopup->OnQuantityConfirmed.Clear();
    ActivePopup->OnQuantityRemove   .Clear();
    ActivePopup->OnQuantityConfirmed.AddDynamic(this, &UVendorShopWidget::HandleSellCartQuantityConfirmed);
    ActivePopup->OnQuantityRemove   .AddDynamic(this, &UVendorShopWidget::HandleSellCartQuantityRemove);
    ActivePopup->OpenForUpdate(SlotIndex, GetLocalizedItemName(Entry.slug), Entry.pricePerUnit, Entry.maxQuantity, Entry.quantity);
}

void UVendorShopWidget::HandleBuyCartSlotRemove(int32 SlotIndex)
{
    if (!BuyCart.IsValidIndex(SlotIndex)) return;
    if (VendorTooltipWidget) VendorTooltipWidget->HideTooltip();
    BuyCart.RemoveAt(SlotIndex);
    RefreshBuyCartDisplay();
    RefreshShopDisplay();
}

void UVendorShopWidget::HandleSellCartSlotRemove(int32 SlotIndex)
{
    if (!SellCart.IsValidIndex(SlotIndex)) return;
    if (VendorTooltipWidget) VendorTooltipWidget->HideTooltip();
    SellCart.RemoveAt(SlotIndex);
    RefreshSellCartDisplay();
    RefreshInventoryDisplay();
}

