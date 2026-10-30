#include "UI/VendorShopWidget.h"
#include "UI/VendorShopWidget.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "UI/VendorTooltipWidget.h"
#include "Utils/NetCore/VendorSellRule.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WrapBox.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "EngineUtils.h"

// S5b: shop total/gold chrome gathered via LOCTEXT (same namespace as Results TU).
#define LOCTEXT_NAMESPACE "VendorShop"

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
            VendorTooltipWidget->SetDataFromCartEntry(Entry, CachedShop.sellTaxPct);
        VendorTooltipWidget->ShowTooltip();
    }
    else VendorTooltipWidget->HideTooltip();
}

void UVendorShopWidget::UpdateBuyTotalText()
{
    if (!Buy_Total_Text) return;
    int32 Total = 0;
    for (const FVendorCartEntry& E : BuyCart) Total += E.pricePerUnit * E.quantity;
    Buy_Total_Text->SetText(FText::Format(LOCTEXT("TotalValue", "Total: {0} g"), FText::AsNumber(Total)));
}

void UVendorShopWidget::UpdateSellTotalText()
{
    if (!Sell_Total_Text) return;
    int32 Total = 0;
    for (const FVendorCartEntry& E : SellCart)
        Total += FVendorSellRule::LineGold(E.pricePerUnit, E.quantity, CachedShop.sellTaxPct);
    Sell_Total_Text->SetText(FText::Format(LOCTEXT("YouGetValue", "You get: {0} g"), FText::AsNumber(Total)));
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
            Delta += FVendorSellRule::LineGold(E.pricePerUnit, E.quantity, CachedShop.sellTaxPct);
    }

    if (Delta == 0)
    {
        Player_Gold_Text->SetText(FText::Format(LOCTEXT("GoldValue", "{0} g"), FText::AsNumber(CurrentGold)));
        Player_Gold_Text->SetColorAndOpacity(FLinearColor(1.f, 0.85f, 0.3f));
    }
    else
    {
        const FString Sign   = (Delta > 0) ? TEXT("+") : TEXT("");
        const FLinearColor C = (Delta > 0) ? FLinearColor(0.3f, 1.f, 0.3f) : FLinearColor(1.f, 0.35f, 0.35f);
        Player_Gold_Text->SetText(FText::Format(LOCTEXT("GoldDelta", "{0} g  ({1}{2} g)"),
            FText::AsNumber(CurrentGold), FText::FromString(Sign), FText::AsNumber(Delta)));
        Player_Gold_Text->SetColorAndOpacity(C);
    }
}

#undef LOCTEXT_NAMESPACE

