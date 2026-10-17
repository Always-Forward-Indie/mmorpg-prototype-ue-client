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

void UVendorShopWidget::HandleBuyQuantityConfirmed(int32 SlotIndex, int32 Quantity)
{
    if (!CachedShop.items.IsValidIndex(SlotIndex)) return;
    const FVendorShopItemData& Item = CachedShop.items[SlotIndex];

    int32 CartIdx = -1;
    if (IsItemInBuyCart(Item.itemId, CartIdx))
    {
        BuyCart[CartIdx].quantity = Quantity;
    }
    else
    {
        FVendorCartEntry Entry;
        Entry.itemId       = Item.itemId;
        Entry.slug         = Item.slug;
        Entry.quantity     = Quantity;
        Entry.pricePerUnit = Item.priceBuy;
        Entry.maxQuantity  = (Item.stockCurrent == -1) ? Item.stackMax : Item.stockCurrent;
        BuyCart.Add(Entry);
    }
    RefreshBuyCartDisplay();
    RefreshShopDisplay();
}

void UVendorShopWidget::HandleBuyQuantityRemove(int32 SlotIndex)
{
    if (!CachedShop.items.IsValidIndex(SlotIndex)) return;
    int32 CartIdx = -1;
    if (IsItemInBuyCart(CachedShop.items[SlotIndex].itemId, CartIdx))
    {
        BuyCart.RemoveAt(CartIdx);
        RefreshBuyCartDisplay();
        RefreshShopDisplay();
    }
}

// Cart-slot popup handlers � SlotIndex is the index inside the cart array

void UVendorShopWidget::HandleBuyCartQuantityConfirmed(int32 CartIndex, int32 Quantity)
{
    if (!BuyCart.IsValidIndex(CartIndex)) return;
    BuyCart[CartIndex].quantity = Quantity;
    RefreshBuyCartDisplay();
    RefreshShopDisplay();
}

void UVendorShopWidget::HandleBuyCartQuantityRemove(int32 CartIndex)
{
    if (!BuyCart.IsValidIndex(CartIndex)) return;
    BuyCart.RemoveAt(CartIndex);
    RefreshBuyCartDisplay();
    RefreshShopDisplay();
}

void UVendorShopWidget::HandleSellQuantityConfirmed(int32 SlotIndex, int32 Quantity)
{
    if (!InvSlots.IsValidIndex(SlotIndex)) return;
    const FInventoryItemStruct Item = InvSlots[SlotIndex]->GetTooltipData();

    int32 CartIdx = -1;
    if (IsItemInSellCart(Item.id, CartIdx))
    {
        SellCart[CartIdx].quantity = Quantity;
    }
    else
    {
        FVendorCartEntry Entry;
        Entry.inventoryItemId = Item.id;
        Entry.slug            = Item.slug;
        Entry.quantity        = Quantity;
        Entry.pricePerUnit    = Item.priceSell;
        Entry.maxQuantity     = Item.quantity;
        SellCart.Add(Entry);
    }
    RefreshSellCartDisplay();
    RefreshInventoryDisplay();
}

void UVendorShopWidget::HandleSellQuantityRemove(int32 SlotIndex)
{
    if (!InvSlots.IsValidIndex(SlotIndex)) return;
    int32 CartIdx = -1;
    if (IsItemInSellCart(InvSlots[SlotIndex]->GetTooltipData().id, CartIdx))
    {
        SellCart.RemoveAt(CartIdx);
        RefreshSellCartDisplay();
        RefreshInventoryDisplay();
    }
}

void UVendorShopWidget::HandleSellCartQuantityConfirmed(int32 CartIndex, int32 Quantity)
{
    if (!SellCart.IsValidIndex(CartIndex)) return;
    SellCart[CartIndex].quantity = Quantity;
    RefreshSellCartDisplay();
    RefreshInventoryDisplay();
}

void UVendorShopWidget::HandleSellCartQuantityRemove(int32 CartIndex)
{
    if (!SellCart.IsValidIndex(CartIndex)) return;
    SellCart.RemoveAt(CartIndex);
    RefreshSellCartDisplay();
    RefreshInventoryDisplay();
}

