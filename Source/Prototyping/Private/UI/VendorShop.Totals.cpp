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

