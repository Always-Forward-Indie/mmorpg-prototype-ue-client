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

void UVendorShopWidget::OpenShop(EVendorTab DefaultTab)
{
    // Reset state on every open
    BuyCart.Reset();
    SellCart.Reset();
    PendingSellExpected = 0;
    PreSellGold = -1;
    PreSellRowQty.Reset();
    if (Status_Text) Status_Text->SetVisibility(ESlateVisibility::Collapsed);

    RefreshShopDisplay();
    RefreshInventoryDisplay();
    RefreshBuyCartDisplay();
    RefreshSellCartDisplay();
    SetVisibility(ESlateVisibility::Visible);
    SwitchToTab(DefaultTab);
    OnVendorShopVisibilityChanged.Broadcast(true);
}

void UVendorShopWidget::CloseShop()
{
    if (ActivePopup) ActivePopup->ClosePopup();
    if (VendorTooltipWidget) VendorTooltipWidget->HideTooltip();
    SetVisibility(ESlateVisibility::Collapsed);
    OnVendorShopVisibilityChanged.Broadcast(false);

    // Notify NPC that this shop window closed; farewell plays if no other windows remain.
    if (CachedShop.npcId > 0)
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UNPCManager* NPCMgr = GI->GetNPCManager())
            {
                if (ABasicNPC* NPC = NPCMgr->GetNPCById(CachedShop.npcId))
                {
                    NPC->NotifyWindowClosed();
                }
            }
        }
    }
}

void UVendorShopWidget::SwitchToTab(EVendorTab Tab)
{
    ActiveTab = Tab;
    if (Buy_Panel)  Buy_Panel ->SetVisibility(Tab == EVendorTab::Buy  ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Sell_Panel) Sell_Panel->SetVisibility(Tab == EVendorTab::Sell ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UpdatePlayerGoldText();
}

void UVendorShopWidget::RefreshShopDisplay()
{
    if (!Buy_Items_Box || !VendorSlotClass) return;

    Buy_Items_Box->ClearChildren();
    ShopSlots.Reset();

    // Pre-compute total gold already committed to the cart for affordability check
    int32 TotalCartCost = 0;
    for (const FVendorCartEntry& E : BuyCart) TotalCartCost += E.pricePerUnit * E.quantity;

    for (int32 i = 0; i < CachedShop.items.Num(); ++i)
    {
        const FVendorShopItemData& Item = CachedShop.items[i];
        UVendorSlotWidget* NewSlot = CreateWidget<UVendorSlotWidget>(GetOwningPlayer(), VendorSlotClass);
        if (!NewSlot) continue;

        int32 CartIdx = -1;
        const bool bIsInCart = IsItemInBuyCart(Item.itemId, CartIdx);

        // Remaining purchasable stock = stockCurrent (-1 = unlimited) minus qty already in cart
        bool bIsFullyInCart = false;
        int32 AlreadyInCart = 0;
        if (bIsInCart)
        {
            AlreadyInCart = BuyCart[CartIdx].quantity;
            if (Item.stockCurrent != -1)
                bIsFullyInCart = (AlreadyInCart >= Item.stockCurrent);
        }

        NewSlot->OnVendorSlotClicked.AddDynamic(this, &UVendorShopWidget::HandleShopSlotClicked);
        NewSlot->OnVendorSlotHovered.AddDynamic(this, &UVendorShopWidget::HandleShopSlotHovered);
        Buy_Items_Box->AddChild(NewSlot);
        NewSlot->SetupAsShopSlot(i, Item, bIsInCart, bIsFullyInCart, AlreadyInCart);
        NewSlot->SetAffordability(GetShopItemAffordability(Item, TotalCartCost));
        ShopSlots.Add(NewSlot);
    }
}

void UVendorShopWidget::RefreshInventoryDisplay()
{
    if (!Sell_Items_Box || !VendorSlotClass) return;

    Sell_Items_Box->ClearChildren();
    InvSlots.Reset();

    int32 VisualIdx = 0;
    for (const FInventoryItemStruct& Item : CachedInventory.items)
    {
        // Skip non-tradable, quest, currency, and equipped items
        if (!Item.isTradable || Item.isQuestItem || Item.is_equipped) continue;
        if (Item.itemTypeSlug.Equals(TEXT("currency"), ESearchCase::IgnoreCase)) continue;
        if (Item.slug.Contains(TEXT("gold"), ESearchCase::IgnoreCase)) continue;

        UVendorSlotWidget* NewSlot = CreateWidget<UVendorSlotWidget>(GetOwningPlayer(), VendorSlotClass);
        if (!NewSlot) continue;

        int32 CartIdx = -1;
        const bool bIsInCart = IsItemInSellCart(Item.id, CartIdx);
        const int32 CartQty = bIsInCart ? SellCart[CartIdx].quantity : 0;
        const bool bIsFullyInCart = bIsInCart && (CartQty >= Item.quantity);

        NewSlot->OnVendorSlotClicked.AddDynamic(this, &UVendorShopWidget::HandleInvSlotClicked);
        NewSlot->OnVendorSlotHovered.AddDynamic(this, &UVendorShopWidget::HandleInvSlotHovered);
        Sell_Items_Box->AddChild(NewSlot);
        NewSlot->SetupAsInvSlot(VisualIdx, Item, bIsInCart, bIsFullyInCart, CartQty);
        InvSlots.Add(NewSlot);
        ++VisualIdx;
    }
}

void UVendorShopWidget::RefreshBuyCartDisplay()
{
    if (!Buy_Cart_Box || !VendorSlotClass) return;
    Buy_Cart_Box->ClearChildren();
    BuyCartSlots.Reset();

    // Total cost of all cart entries � used to check if player can afford everything
    int32 TotalCartCost = 0;
    for (const FVendorCartEntry& E : BuyCart) TotalCartCost += E.pricePerUnit * E.quantity;

    for (int32 i = 0; i < BuyCart.Num(); ++i)
    {
        UVendorSlotWidget* NewSlot = CreateWidget<UVendorSlotWidget>(GetOwningPlayer(), VendorSlotClass);
        if (!NewSlot) continue;
        NewSlot->OnVendorSlotClicked.AddDynamic(this, &UVendorShopWidget::HandleBuyCartSlotClicked);
        NewSlot->OnVendorSlotHovered.AddDynamic(this, &UVendorShopWidget::HandleBuyCartSlotHovered);
        NewSlot->OnRemoveFromCart   .AddDynamic(this, &UVendorShopWidget::HandleBuyCartSlotRemove);
        Buy_Cart_Box->AddChild(NewSlot);
        NewSlot->SetupAsCartSlot(i, BuyCart[i]);
        NewSlot->SetAffordability(GetCartEntryAffordability(TotalCartCost));
        BuyCartSlots.Add(NewSlot);
    }

    // Keep the confirm button in sync with cart content and affordability
    if (Buy_Confirm_Button)
        Buy_Confirm_Button->SetIsEnabled(BuyCart.Num() > 0 && TotalCartCost <= CachedInventory.gold);

    UpdateBuyTotalText();
    UpdatePlayerGoldText();
}

void UVendorShopWidget::RefreshSellCartDisplay()
{
    if (!Sell_Cart_Box || !VendorSlotClass) return;
    Sell_Cart_Box->ClearChildren();
    SellCartSlots.Reset();

    for (int32 i = 0; i < SellCart.Num(); ++i)
    {
        UVendorSlotWidget* NewSlot = CreateWidget<UVendorSlotWidget>(GetOwningPlayer(), VendorSlotClass);
        if (!NewSlot) continue;
        NewSlot->OnVendorSlotClicked.AddDynamic(this, &UVendorShopWidget::HandleSellCartSlotClicked);
        NewSlot->OnVendorSlotHovered.AddDynamic(this, &UVendorShopWidget::HandleSellCartSlotHovered);
        NewSlot->OnRemoveFromCart   .AddDynamic(this, &UVendorShopWidget::HandleSellCartSlotRemove);
        Sell_Cart_Box->AddChild(NewSlot);
        NewSlot->SetupAsCartSlot(i, SellCart[i]);
        NewSlot->SetSellLineTax(CachedShop.sellTaxPct);
        SellCartSlots.Add(NewSlot);
    }

    // Keep the confirm button in sync with cart content so it enables
    // automatically whenever a new item is added, even after a prior sale cleared the cart.
    if (Sell_Confirm_Button)
        Sell_Confirm_Button->SetIsEnabled(SellCart.Num() > 0);

    UpdateSellTotalText();
    UpdatePlayerGoldText();
}

void UVendorShopWidget::HandleTabBuyClicked()  { SwitchToTab(EVendorTab::Buy);  }
void UVendorShopWidget::HandleTabSellClicked() { SwitchToTab(EVendorTab::Sell); }

