#include "UI/VendorShopWidget.h"
#include "UI/VendorShopWidget.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "UI/VendorTooltipWidget.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/WrapBox.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UVendorShopWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleCloseButtonClicked);
    if (Tab_Buy_Button)
        Tab_Buy_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleTabBuyClicked);
    if (Tab_Sell_Button)
        Tab_Sell_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleTabSellClicked);
    if (Buy_Confirm_Button)
        Buy_Confirm_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleConfirmBuyCart);
    if (Sell_Confirm_Button)
        Sell_Confirm_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleConfirmSellCart);
    if (Buy_Clear_Button)
        Buy_Clear_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleClearBuyCart);
    if (Sell_Clear_Button)
        Sell_Clear_Button->OnClicked.AddDynamic(this, &UVendorShopWidget::HandleClearSellCart);

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

    // Create the tooltip widget and put it on top of everything
    if (VendorTooltipWidgetClass)
    {
        VendorTooltipWidget = CreateWidget<UVendorTooltipWidget>(GetOwningPlayer(), VendorTooltipWidgetClass);
        if (VendorTooltipWidget)
            VendorTooltipWidget->AddToViewport(1000);
    }
}

void UVendorShopWidget::NativeDestruct()
{
    if (ActivePopup)
    {
        ActivePopup->ClosePopup();
        ActivePopup->RemoveFromParent();
        ActivePopup = nullptr;
    }
    if (VendorTooltipWidget)
    {
        VendorTooltipWidget->RemoveFromParent();
        VendorTooltipWidget = nullptr;
    }
    Super::NativeDestruct();
}





void UVendorShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!VendorTooltipWidget) return;
    const ESlateVisibility Vis = VendorTooltipWidget->GetVisibility();
    if (Vis != ESlateVisibility::HitTestInvisible && Vis != ESlateVisibility::SelfHitTestInvisible) return;

    VendorTooltipWidget->UpdateTooltipPosition();
}

void UVendorShopWidget::BindToManagers(UVendorManager* InVendorManager, UInventoryManager* InInventoryManager)
{
    if (!InVendorManager) return;

    // Unbind old
    if (VendorManager)
    {
        VendorManager->OnVendorShopOpenedDelegate   .RemoveDynamic(this, &UVendorShopWidget::HandleVendorShopOpened);
        VendorManager->OnBuyItemResultDelegate      .RemoveDynamic(this, &UVendorShopWidget::HandleBuyItemResult);
        VendorManager->OnSellItemResultDelegate     .RemoveDynamic(this, &UVendorShopWidget::HandleSellItemResult);
        VendorManager->OnBuyItemBatchResultDelegate .RemoveDynamic(this, &UVendorShopWidget::HandleBuyItemBatchResult);
        VendorManager->OnSellItemBatchResultDelegate.RemoveDynamic(this, &UVendorShopWidget::HandleSellItemBatchResult);
    }
    if (InventoryManager)
        InventoryManager->OnInventoryUpdated.RemoveDynamic(this, &UVendorShopWidget::HandleInventoryUpdated);

    VendorManager    = InVendorManager;
    InventoryManager = InInventoryManager;

    VendorManager->OnVendorShopOpenedDelegate   .AddDynamic(this, &UVendorShopWidget::HandleVendorShopOpened);
    VendorManager->OnBuyItemResultDelegate      .AddDynamic(this, &UVendorShopWidget::HandleBuyItemResult);
    VendorManager->OnSellItemResultDelegate     .AddDynamic(this, &UVendorShopWidget::HandleSellItemResult);
    VendorManager->OnBuyItemBatchResultDelegate .AddDynamic(this, &UVendorShopWidget::HandleBuyItemBatchResult);
    VendorManager->OnSellItemBatchResultDelegate.AddDynamic(this, &UVendorShopWidget::HandleSellItemBatchResult);

    if (InventoryManager)
    {
        InventoryManager->OnInventoryUpdated.AddDynamic(this, &UVendorShopWidget::HandleInventoryUpdated);
        CachedInventory = InventoryManager->GetInventory();
    }
}

void UVendorShopWidget::OpenShop(EVendorTab DefaultTab)
{
    // Reset state on every open
    BuyCart.Reset();
    SellCart.Reset();
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
        // Skip non-tradable, quest and currency items
        if (!Item.isTradable || Item.isQuestItem) continue;
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

    // Total cost of all cart entries — used to check if player can afford everything
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
        SellCartSlots.Add(NewSlot);
    }
    UpdateSellTotalText();
    UpdatePlayerGoldText();
}

void UVendorShopWidget::HandleTabBuyClicked()  { SwitchToTab(EVendorTab::Buy);  }
void UVendorShopWidget::HandleTabSellClicked() { SwitchToTab(EVendorTab::Sell); }

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
    // stockCurrent >= 0 means finite stock — that is the hard ceiling
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

// Cart-slot popup handlers — SlotIndex is the index inside the cart array

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

void UVendorShopWidget::HandleConfirmBuyCart()
{
    if (!VendorManager || BuyCart.IsEmpty()) return;
    if (Buy_Confirm_Button) Buy_Confirm_Button->SetIsEnabled(false);
    VendorManager->RequestBuyItemBatch(GetCharacterId(), CachedShop.npcId, BuyCart, GetPlayerPosition());
}

void UVendorShopWidget::HandleConfirmSellCart()
{
    if (!VendorManager || SellCart.IsEmpty()) return;
    if (Sell_Confirm_Button) Sell_Confirm_Button->SetIsEnabled(false);
    VendorManager->RequestSellItemBatch(GetCharacterId(), CachedShop.npcId, SellCart, GetPlayerPosition());
}

void UVendorShopWidget::HandleClearBuyCart()
{
    BuyCart.Reset();
    RefreshBuyCartDisplay();
    RefreshShopDisplay();
}

void UVendorShopWidget::HandleClearSellCart()
{
    SellCart.Reset();
    RefreshSellCartDisplay();
    RefreshInventoryDisplay();
}

void UVendorShopWidget::HandleVendorShopOpened(const FVendorShopData& ShopData)
{
    if (ShopData.npcId != CachedShop.npcId)
    {
        BuyCart.Reset();
        SellCart.Reset();
    }
    CachedShop = ShopData;
    if (NPC_Name_Text)
    {
        FString DisplayName = ShopData.npcName;

        // If the server didn't send a name, try to resolve it from a spawned NPC actor by slug
        if (DisplayName.IsEmpty() && !ShopData.npcSlug.IsEmpty() && GetWorld())
        {
            for (TActorIterator<ABasicNPC> It(GetWorld()); It; ++It)
            {
                if (It->GetNPCSlug().Equals(ShopData.npcSlug, ESearchCase::IgnoreCase))
                {
                    DisplayName = It->GetNPCName();
                    break;
                }
            }
        }

        // Final fallback: use slug with capitalised first letter, or NPC id
        if (DisplayName.IsEmpty())
        {
            if (!ShopData.npcSlug.IsEmpty())
            {
                DisplayName = ShopData.npcSlug;
                if (DisplayName.Len() > 0)
                    DisplayName[0] = FChar::ToUpper(DisplayName[0]);
            }
            else
            {
                DisplayName = FString::Printf(TEXT("Vendor (NPC #%d)"), ShopData.npcId);
            }
        }

        NPC_Name_Text->SetText(FText::FromString(DisplayName));
    }
    OpenShop(EVendorTab::Buy);
}

void UVendorShopWidget::HandleBuyItemResult(const FBuyItemResultData& Result)
{
    if (!Result.errorCode.IsEmpty())
        ShowStatus(FString::Printf(TEXT("Buy failed: %s"), *Result.errorCode));
    else
        ShowStatus(FString::Printf(TEXT("Bought x%d  (-%d g)"), Result.quantity, Result.totalPrice));
    RefreshShopDisplay();
}

void UVendorShopWidget::HandleSellItemResult(const FSellItemResultData& Result)
{
    if (!Result.errorCode.IsEmpty())
        ShowStatus(FString::Printf(TEXT("Sell failed: %s"), *Result.errorCode));
    else
        ShowStatus(FString::Printf(TEXT("Sold  (+%d g)"), Result.goldReceived));
    RefreshInventoryDisplay();
}

void UVendorShopWidget::HandleBuyItemBatchResult(const FBuyItemBatchResultData& Result)
{
    if (Buy_Confirm_Button) Buy_Confirm_Button->SetIsEnabled(true);
    if (!Result.errorCode.IsEmpty())
    {
        ShowStatus(FString::Printf(TEXT("Buy failed: %s"), *Result.errorCode));
        return;
    }
    ShowStatus(FString::Printf(TEXT("Bought %d items  (-%d g)"), Result.items.Num(), Result.totalGoldSpent));
    BuyCart.Reset();
    RefreshBuyCartDisplay();
    RefreshShopDisplay(); // stock may have changed
}

void UVendorShopWidget::HandleSellItemBatchResult(const FSellItemBatchResultData& Result)
{
    if (Sell_Confirm_Button) Sell_Confirm_Button->SetIsEnabled(true);
    if (!Result.errorCode.IsEmpty())
    {
        ShowStatus(FString::Printf(TEXT("Sell failed: %s"), *Result.errorCode));
        return;
    }
    ShowStatus(FString::Printf(TEXT("Sold %d items  (+%d g)"), Result.items.Num(), Result.totalGoldReceived));

    // Update CachedInventory quantities based on what was actually sold
    for (const FSellBatchItemResult& SoldItem : Result.items)
    {
        for (int32 i = CachedInventory.items.Num() - 1; i >= 0; --i)
        {
            if (CachedInventory.items[i].id == SoldItem.inventoryItemId)
            {
                CachedInventory.items[i].quantity -= SoldItem.quantity;
                if (CachedInventory.items[i].quantity <= 0)
                    CachedInventory.items.RemoveAt(i);
                break;
            }
        }
    }

    SellCart.Reset();
    RefreshSellCartDisplay();
    RefreshInventoryDisplay();
}

void UVendorShopWidget::HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory)
{
    CachedInventory = Inventory;
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        RefreshInventoryDisplay();
        UpdatePlayerGoldText();
    }
}

void UVendorShopWidget::HandleCloseButtonClicked() { CloseShop(); }

void UVendorShopWidget::ShowStatus(const FString& Msg)
{
    if (Status_Text)
    {
        Status_Text->SetText(FText::FromString(Msg));
        Status_Text->SetVisibility(ESlateVisibility::Visible);
    }
}

void UVendorShopWidget::HandleBuyCartSlotHovered(int32 SlotIndex, bool bIsHovered)
{
    if (!VendorTooltipWidget) return;
    if (bIsHovered && BuyCartSlots.IsValidIndex(SlotIndex))
    {
        const FVendorCartEntry& Entry = BuyCart[SlotIndex];
        // Find the full item data from the cached shop catalogue by itemId
        const FVendorShopItemData* Found = CachedShop.items.FindByPredicate(
            [&Entry](const FVendorShopItemData& ShopItem)
            {
                return ShopItem.itemId == Entry.itemId;
            });
        if (Found)
            VendorTooltipWidget->SetDataFromShopItem(*Found);
        else
            VendorTooltipWidget->SetDataFromCartEntry(Entry);
        VendorTooltipWidget->ShowTooltip();
    }
    else VendorTooltipWidget->HideTooltip();
}

void UVendorShopWidget::HandleSellCartSlotHovered(int32 SlotIndex, bool bIsHovered)
{
    if (!VendorTooltipWidget) return;
    if (bIsHovered && SellCartSlots.IsValidIndex(SlotIndex))
    {
        const FVendorCartEntry& Entry = SellCart[SlotIndex];
        // Find the full item data from the cached inventory by inventoryItemId
        const FInventoryItemStruct* Found = CachedInventory.items.FindByPredicate(
            [&Entry](const FInventoryItemStruct& InvItem)
            {
                return InvItem.id == Entry.inventoryItemId;
            });
        if (Found)
            VendorTooltipWidget->SetDataFromInventoryItem(*Found);
        else
            VendorTooltipWidget->SetDataFromCartEntry(Entry);
        VendorTooltipWidget->ShowTooltip();
    }
    else VendorTooltipWidget->HideTooltip();
}

void UVendorShopWidget::UpdateBuyTotalText()
{
    if (!Buy_Total_Text) return;
    int32 Total = 0;
    for (const FVendorCartEntry& E : BuyCart) Total += E.pricePerUnit * E.quantity;
    Buy_Total_Text->SetText(FText::FromString(FString::Printf(TEXT("Total: %d g"), Total)));
}

void UVendorShopWidget::UpdateSellTotalText()
{
    if (!Sell_Total_Text) return;
    int32 Total = 0;
    for (const FVendorCartEntry& E : SellCart) Total += E.pricePerUnit * E.quantity;
    Sell_Total_Text->SetText(FText::FromString(FString::Printf(TEXT("You get: %d g"), Total)));
}

void UVendorShopWidget::UpdatePlayerGoldText()
{
    if (!Player_Gold_Text) return;

    const int32 CurrentGold = CachedInventory.gold;

    // Calculate cart delta for whichever tab is active
    int32 Delta = 0;
    if (ActiveTab == EVendorTab::Buy)
    {
        for (const FVendorCartEntry& E : BuyCart)
            Delta -= E.pricePerUnit * E.quantity;
    }
    else
    {
        for (const FVendorCartEntry& E : SellCart)
            Delta += E.pricePerUnit * E.quantity;
    }

    if (Delta == 0)
    {
        Player_Gold_Text->SetText(FText::FromString(FString::Printf(TEXT("%d g"), CurrentGold)));
        Player_Gold_Text->SetColorAndOpacity(FLinearColor(1.f, 0.85f, 0.3f));
    }
    else
    {
        const FString Sign   = (Delta > 0) ? TEXT("+") : TEXT("");
        const FLinearColor C = (Delta > 0) ? FLinearColor(0.3f, 1.f, 0.3f) : FLinearColor(1.f, 0.35f, 0.35f);
        Player_Gold_Text->SetText(FText::FromString(
            FString::Printf(TEXT("%d g  (%s%d g)"), CurrentGold, *Sign, Delta)));
        Player_Gold_Text->SetColorAndOpacity(C);
    }
}

void UVendorShopWidget::EnsurePopup()
{
    if (!ActivePopup && QuantityPopupClass)
    {
        ActivePopup = CreateWidget<UQuantityPopupWidget>(GetOwningPlayer(), QuantityPopupClass);
        if (ActivePopup)
            ActivePopup->AddToViewport(999);
    }
}

bool UVendorShopWidget::IsItemInBuyCart(int32 ItemId, int32& OutCartIndex) const
{
    for (int32 i = 0; i < BuyCart.Num(); ++i)
        if (BuyCart[i].itemId == ItemId) { OutCartIndex = i; return true; }
    OutCartIndex = -1;
    return false;
}

bool UVendorShopWidget::IsItemInSellCart(int32 InventoryItemId, int32& OutCartIndex) const
{
    for (int32 i = 0; i < SellCart.Num(); ++i)
        if (SellCart[i].inventoryItemId == InventoryItemId) { OutCartIndex = i; return true; }
    OutCartIndex = -1;
    return false;
}

// ---------------------------------------------------------------------------
// Affordability helpers
// ---------------------------------------------------------------------------

int32 UVendorShopWidget::GetPlayerLevel() const
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) return 0;
    UPlayerStatsManager* PSM = GI->GetPlayerStatsManager();
    return PSM ? PSM->GetCachedStats().level : 0;
}

EVendorSlotAffordability UVendorShopWidget::GetShopItemAffordability(const FVendorShopItemData& Item, int32 TotalCartCost) const
{
    // Level check takes priority over gold — more informative to the player
    const int32 PlayerLevel = GetPlayerLevel();
    if (PlayerLevel > 0 && Item.levelRequirement > PlayerLevel)
        return EVendorSlotAffordability::LevelTooLow;

    // Gold check: current gold minus what is already committed to the cart
    const int32 AvailableGold = CachedInventory.gold - TotalCartCost;
    if (Item.priceBuy > AvailableGold)
        return EVendorSlotAffordability::NotEnoughGold;

    return EVendorSlotAffordability::Affordable;
}

EVendorSlotAffordability UVendorShopWidget::GetCartEntryAffordability(int32 TotalBuyCartCost) const
{
    if (TotalBuyCartCost > CachedInventory.gold)
        return EVendorSlotAffordability::NotEnoughGold;
    return EVendorSlotAffordability::Affordable;
}

int32 UVendorShopWidget::GetCharacterId() const
{
    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    return GI ? GI->GetCurrentCharacterID() : 0;
}



FVector UVendorShopWidget::GetPlayerPosition() const
{
    if (APlayerController* PC = GetOwningPlayer())
        if (APawn* Pawn = PC->GetPawn())
            return Pawn->GetActorLocation();
    return FVector::ZeroVector;
}

FString UVendorShopWidget::GetLocalizedItemName(const FString& ItemSlug) const
{
    if (!ItemSlug.IsEmpty())
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                const FText LocName = Loc->GetItemDisplayName(ItemSlug);
                if (!LocName.IsEmpty()) return LocName.ToString();
            }
        }
    }
    return ItemSlug;
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

FReply UVendorShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

FReply UVendorShopWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

FReply UVendorShopWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging) { UpdateWindowDragPosition(InMouseEvent.GetScreenSpacePosition()); return FReply::Handled(); }
    return FReply::Unhandled();
}

void UVendorShopWidget::UpdateWindowDragPosition(const FVector2D& ScreenCursorPos)
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
    const FVector2D VP = FVector2D(W, H) / Scale;
    ForceLayoutPrepass();
    FVector2D Size = GetDesiredSize();
    if (Size.IsZero()) Size = FVector2D(500, 600);
    FVector2D Pos = ScreenCursorPos / Scale - DragOffset;
    Pos.X = FMath::Clamp(Pos.X, 0.f, VP.X - Size.X);
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, VP.Y - Size.Y);
    CurrentViewportPosition = Pos;
    SetPositionInViewport(Pos, false);
}

