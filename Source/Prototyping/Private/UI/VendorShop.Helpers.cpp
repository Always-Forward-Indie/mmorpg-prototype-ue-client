#include "UI/VendorShopWidget.h"
#include "UI/VendorShopWidget.h"
#include "Utils/NetCore/VendorAffordability.h"
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
    // Predicate lives in FVendorAffordability (pinned by MMO.Shop.Affordability).
    return FVendorAffordability::ForShopItem(
        GetPlayerLevel(), Item.levelRequirement, Item.priceBuy, CachedInventory.gold, TotalCartCost);
}

EVendorSlotAffordability UVendorShopWidget::GetCartEntryAffordability(int32 TotalBuyCartCost) const
{
    // Predicate lives in FVendorAffordability (pinned by MMO.Shop.Affordability).
    return FVendorAffordability::ForCartEntry(TotalBuyCartCost, CachedInventory.gold);
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

FString UVendorShopWidget::ResolveItemNameFromShop(int32 ItemId) const
{
    for (const FVendorShopItemData& ShopItem : CachedShop.items)
    {
        if (ShopItem.itemId == ItemId)
            return GetLocalizedItemName(ShopItem.itemSlug);
    }
    return TEXT("Item");
}

FString UVendorShopWidget::ResolveItemNameFromInventory(int32 InventoryItemId) const
{
    for (const FInventoryItemStruct& InvItem : CachedInventory.items)
    {
        if (InvItem.id == InventoryItemId)
            return GetLocalizedItemName(InvItem.itemSlug);
    }
    return TEXT("Item");
}

// ---------------------------------------------------------------------------
// Drag support
// ---------------------------------------------------------------------------

