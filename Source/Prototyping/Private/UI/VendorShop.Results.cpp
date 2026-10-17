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

    // Optimistic UI update: clear the cart immediately so the seller sees instant feedback.
    // The server response (sellItemBatchResult or getPlayerInventory) will correct
    // CachedInventory to the authoritative state if anything differs.
    SellCart.Reset();
    RefreshSellCartDisplay();
    UpdatePlayerGoldText();
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

    // Notify NPC that the shop window opened so the farewell counter stays balanced.
    if (CachedShop.npcId > 0)
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UNPCManager* NPCMgr = GI->GetNPCManager())
            {
                if (ABasicNPC* NPC = NPCMgr->GetNPCById(CachedShop.npcId))
                {
                    NPC->NotifyWindowOpened();
                }
            }
        }
    }

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
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FString::Printf(TEXT("Buy failed: %s"), *Friendly.ToString()));
	}
	else
	{
		FString ItemName = ResolveItemNameFromShop(Result.itemId);
		ShowStatus(FString::Printf(TEXT("Bought %s x%d  (-%d g)"), *ItemName, Result.quantity, Result.totalPrice));
	}
	RefreshShopDisplay();
}

void UVendorShopWidget::HandleSellItemResult(const FSellItemResultData& Result)
{
	if (!Result.errorCode.IsEmpty())
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FString::Printf(TEXT("Sell failed: %s"), *Friendly.ToString()));
	}
	else
	{
		FString ItemName = ResolveItemNameFromInventory(Result.inventoryItemId);
		ShowStatus(FString::Printf(TEXT("Sold %s  (+%d g)"), *ItemName, Result.goldReceived));
	}
	RefreshInventoryDisplay();
}

void UVendorShopWidget::HandleBuyItemBatchResult(const FBuyItemBatchResultData& Result)
{
	if (Buy_Confirm_Button) Buy_Confirm_Button->SetIsEnabled(true);
	if (!Result.errorCode.IsEmpty())
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FString::Printf(TEXT("Buy failed: %s"), *Friendly.ToString()));
		return;
	}
	FString FirstItemName;
	if (Result.items.Num() > 0)
		FirstItemName = ResolveItemNameFromShop(Result.items[0].itemId);
	if (Result.items.Num() <= 1)
		ShowStatus(FString::Printf(TEXT("Bought %s  (-%d g)"), *FirstItemName, Result.totalGoldSpent));
	else
		ShowStatus(FString::Printf(TEXT("Bought %s [+%d more]  (-%d g)"), *FirstItemName, Result.items.Num() - 1, Result.totalGoldSpent));
	BuyCart.Reset();
	RefreshBuyCartDisplay();
	RefreshShopDisplay(); // stock may have changed
}

void UVendorShopWidget::HandleSellItemBatchResult(const FSellItemBatchResultData& Result)
{
	if (Sell_Confirm_Button) Sell_Confirm_Button->SetIsEnabled(true);
	if (!Result.errorCode.IsEmpty())
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FString::Printf(TEXT("Sell failed: %s"), *Friendly.ToString()));
		RefreshSellCartDisplay();
		RefreshInventoryDisplay();
		return;
	}
    FString FirstItemName;
    if (Result.items.Num() > 0)
        FirstItemName = ResolveItemNameFromInventory(Result.items[0].inventoryItemId);
    if (Result.items.Num() <= 1)
        ShowStatus(FString::Printf(TEXT("Sold %s  (+%d g)"), *FirstItemName, Result.totalGoldReceived));
    else
        ShowStatus(FString::Printf(TEXT("Sold %s [+%d more]  (+%d g)"), *FirstItemName, Result.items.Num() - 1, Result.totalGoldReceived));

    // Update CachedInventory quantities based on what was actually sold.
    // Only subtract if the item still exists — getPlayerInventory may have already
    // replaced CachedInventory with the post-sale state before this result arrived.
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

    // Server-authoritative gold: credit the player immediately so the UI doesn't
    // stall waiting for the next getPlayerInventory packet.
    CachedInventory.gold += Result.totalGoldReceived;

    SellCart.Reset();
    RefreshSellCartDisplay();
    RefreshInventoryDisplay();
    UpdatePlayerGoldText();
}

void UVendorShopWidget::HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory)
{
    CachedInventory = Inventory;

    // Remove sell cart entries for items that are no longer in the inventory.
    // The server typically responds to a successful sell with getPlayerInventory
    // (instead of a dedicated sell-result event), so this keeps the cart in sync.
    for (int32 i = SellCart.Num() - 1; i >= 0; --i)
    {
        const int32 CartInvId = SellCart[i].inventoryItemId;
        const bool bStillExists = CachedInventory.items.ContainsByPredicate(
            [CartInvId](const FInventoryItemStruct& InvItem){ return InvItem.id == CartInvId && !InvItem.is_equipped; });
        if (!bStillExists)
            SellCart.RemoveAt(i);
    }

    if (GetVisibility() == ESlateVisibility::Visible)
    {
        RefreshSellCartDisplay();
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

