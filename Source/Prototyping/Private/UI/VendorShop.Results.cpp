#include "UI/VendorShopWidget.h"
#include "UI/VendorShopWidget.h"
#include "Prototyping.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "UI/VendorTooltipWidget.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationStackWidget.h"
#include "Utils/NetCore/VendorSellRule.h"
#include "Utils/NetCore/VendorResultMatchRule.h"
#include "Audio/AudioManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WrapBox.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "EngineUtils.h"

// S5b: shop status/result chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "VendorShop"

void UVendorShopWidget::HandleConfirmBuyCart()
{
    if (!VendorManager || BuyCart.IsEmpty()) return;
    if (Buy_Confirm_Button) Buy_Confirm_Button->SetIsEnabled(false);
    VendorManager->RequestBuyItemBatch(GetCharacterId(), CachedShop.npcId, BuyCart, GetPlayerPosition());
    PendingBuyBatchRequestId = VendorManager->GetLastBuyBatchRequestId();
    bBuyBatchApplied = false;
}

void UVendorShopWidget::HandleConfirmSellCart()
{
    if (!VendorManager || SellCart.IsEmpty()) return;
    if (Sell_Confirm_Button) Sell_Confirm_Button->SetIsEnabled(false);
    VendorManager->RequestSellItemBatch(GetCharacterId(), CachedShop.npcId, SellCart, GetPlayerPosition());
    PendingSellBatchRequestId = VendorManager->GetLastSellBatchRequestId();
    bSellBatchApplied = false;

    // Remember the expected gain so the totals keep showing it (as pending)
    // instead of dropping to an empty-cart zero before the server answers.
    PendingSellExpected = 0;
    for (const FVendorCartEntry& E : SellCart)
        PendingSellExpected += FVendorSellRule::LineGold(E.pricePerUnit, E.quantity, CachedShop.sellTaxPct);

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

void UVendorShopWidget::SellJunkItems()
{
    if (!VendorManager || !InventoryManager) return;
    TArray<FVendorCartEntry> JunkCart;
    for (const FInventoryItemStruct& Item : CachedInventory.items)
    {
        if (Item.id <= 0 || Item.quantity <= 0 || Item.is_equipped) continue;
        const FString Slug = !Item.itemSlug.IsEmpty() ? Item.itemSlug : Item.slug;
        if (Slug.IsEmpty() || !InventoryManager->IsJunkSlug(Slug)) continue;
        FVendorCartEntry E;
        E.itemId = Item.itemId;
        E.inventoryItemId = Item.id;
        E.itemSlug = Slug;
        E.itemName = Item.name;
        E.slug = Slug;
        E.quantity = Item.quantity;
        E.maxQuantity = Item.quantity;
        E.pricePerUnit = Item.priceSell;
        JunkCart.Add(E);
    }
    if (JunkCart.IsEmpty())
    {
        ShowStatus(LOCTEXT("NoJunkMarked", "No junk marked — mark items via the inventory menu."), EShopStatusSeverity::Info);
        return;
    }
    VendorManager->RequestSellItemBatch(GetCharacterId(), CachedShop.npcId, JunkCart, GetPlayerPosition());
    PendingSellBatchRequestId = VendorManager->GetLastSellBatchRequestId();
    bSellBatchApplied = false;
    PendingSellExpected = 0;
    for (const FVendorCartEntry& E : JunkCart)
        PendingSellExpected += FVendorSellRule::LineGold(E.pricePerUnit, E.quantity, CachedShop.sellTaxPct);
}

void UVendorShopWidget::OpenBuybackTab()
{
    if (!VendorManager || CachedShop.npcId <= 0) return;
    SwitchToTab(EVendorTab::Buyback);
}

void UVendorShopWidget::HandleBuybackList(const FBuybackListData& List)
{
    // Store the snapshot and paint the tab (requested on tab enter).
    // Never re-request here — that would loop request → list → request.
    BuybackEntries = List.entries;
    RefreshBuybackDisplay();
}

void UVendorShopWidget::RefreshBuybackDisplay()
{
    if (!Buyback_Items_Box || !VendorSlotClass) return;
    Buyback_Items_Box->ClearChildren();
    BuybackSlots.Reset();

    if (BuybackEntries.Num() == 0)
    {
        ShowStatus(LOCTEXT("BuybackEmpty", "Nothing to buy back"), EShopStatusSeverity::Info);
        return;
    }

    for (int32 i = 0; i < BuybackEntries.Num(); ++i)
    {
        const FBuybackEntry& Entry = BuybackEntries[i];
        UVendorSlotWidget* NewSlot = CreateWidget<UVendorSlotWidget>(GetOwningPlayer(), VendorSlotClass);
        if (!NewSlot) continue;
        NewSlot->SetupAsCartSlot(i, BuybackEntryToCart(Entry, LocalizeBuybackName(Entry.slug)));
        // Display-only cart: no [X] button, click = buy the whole stack.
        NewSlot->SetAllowRemoveButton(false);
        NewSlot->OnVendorSlotClicked.AddDynamic(this, &UVendorShopWidget::HandleBuybackSlotClicked);
        NewSlot->OnVendorSlotHovered.AddDynamic(this, &UVendorShopWidget::HandleBuybackSlotHovered);
        BuybackSlots.Add(NewSlot);
        Buyback_Items_Box->AddChild(NewSlot);
    }
}

FVendorCartEntry UVendorShopWidget::BuybackEntryToCart(const FBuybackEntry& Entry, const FString& DisplayName) const
{
    FVendorCartEntry E;
    E.itemId = Entry.itemId;
    E.inventoryItemId = 0; // sold stack: no live inventory row
    E.itemSlug = Entry.slug;
    E.itemName = DisplayName;
    E.slug = Entry.slug;
    E.quantity = Entry.quantity;
    E.maxQuantity = Entry.quantity;
    E.pricePerUnit = Entry.unitPrice;
    return E;
}

FString UVendorShopWidget::LocalizeBuybackName(const FString& Slug) const
{
    if (Slug.IsEmpty()) return TEXT("—");
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            const FText LocName = Loc->GetItemDisplayName(Slug);
            if (!LocName.IsEmpty())
                return LocName.ToString();
        }
    }
    return Slug;
}

void UVendorShopWidget::HandleBuybackSlotClicked(int32 SlotIndex)
{
    if (!VendorManager || CachedShop.npcId <= 0) return;
    if (!BuybackEntries.IsValidIndex(SlotIndex)) return;
    const FBuybackEntry& Entry = BuybackEntries[SlotIndex];
    if (Entry.quantity <= 0) return;
    // Full-stack repurchase (v1: no partial quantities).
    VendorManager->RequestBuybackItem(GetCharacterId(), CachedShop.npcId, Entry.slot, Entry.quantity, GetPlayerPosition());
}

void UVendorShopWidget::HandleBuybackSlotHovered(int32 SlotIndex, bool bIsHovered)
{
    if (!VendorTooltipWidget) return;
    if (bIsHovered && BuybackSlots.IsValidIndex(SlotIndex) && BuybackSlots[SlotIndex])
    {
        VendorTooltipWidget->SetDataFromCartEntry(BuybackSlots[SlotIndex]->GetCartEntryData());
        VendorTooltipWidget->ShowTooltip();
    }
    else
        VendorTooltipWidget->HideTooltip();
}

void UVendorShopWidget::HandleBuybackResult(const FBuybackResultData& Result)
{
    if (Result.bSuccess)
    {
        ShowStatus(FText::FromString(FString::Printf(TEXT("Bought back x%d for %d g"), Result.quantity, Result.pricePaid)), EShopStatusSeverity::Success);
        // Refresh the list (entry shrank or vanished); gold/items refresh
        // arrives through the regular inventory update broadcast.
        if (VendorManager)
            VendorManager->RequestBuybackList(GetCharacterId());
    }
    else if (!Result.errorCode.IsEmpty())
    {
        ShowStatus(FText::FromString(FString::Printf(TEXT("Buyback failed: %s"), *Result.errorCode)), EShopStatusSeverity::Error);
    }
}

void UVendorShopWidget::SendShopToast(const FString& NotificationType, const TMap<FString, FString>& DataFields)
{
    const APlayerController* PC = GetOwningPlayer();
    const ABasicPlayer* Pawn = PC ? Cast<ABasicPlayer>(PC->GetPawn()) : nullptr;
    UUIManager* UIMgr = Pawn ? Pawn->GetUIManager() : nullptr;
    UWorldNotificationManager* NotifMgr = UIMgr ? UIMgr->GetWorldNotificationManager() : nullptr;
    UNotificationStackWidget* Stack = NotifMgr ? NotifMgr->GetToastWidget() : nullptr;
    if (Stack)
    {
        Stack->EnqueueActionToast(NotificationType, DataFields);
    }
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
                DisplayName = FText::Format(LOCTEXT("VendorNumbered", "Vendor (NPC #{0})"),
                    FText::AsNumber(ShopData.npcId)).ToString();
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
		ShowStatus(FText::Format(LOCTEXT("BuyFailed", "Buy failed: {0}"), Friendly), EShopStatusSeverity::Error);
	}
	else
	{
		FString ItemName = ResolveItemNameFromShop(Result.itemId);
		if (Result.quantity > 0 && Result.totalPrice == 0)
		{
			UE_LOG(LogGameplay, Warning, TEXT("VendorShop: buy success pays 0 (npc=%d item=%d qty=%d)"),
				CachedShop.npcId, Result.itemId, Result.quantity);
		}
		ShowStatus(FText::Format(LOCTEXT("BoughtQty", "Bought {0} x{1}  (-{2} g)"),
			FText::FromString(ItemName), FText::AsNumber(Result.quantity), FText::AsNumber(Result.totalPrice)), EShopStatusSeverity::Success);
		TMap<FString, FString> ToastFields;
		ToastFields.Add(TEXT("itemId"), FString::FromInt(Result.itemId));
		ToastFields.Add(TEXT("itemSlug"), ResolveItemSlugFromShop(Result.itemId));
		ToastFields.Add(TEXT("quantity"), FString::FromInt(Result.quantity));
		SendShopToast(TEXT("item_received"), ToastFields);
	}
	RefreshShopDisplay();
}

void UVendorShopWidget::HandleSellItemResult(const FSellItemResultData& Result)
{
	if (!Result.errorCode.IsEmpty())
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FText::Format(LOCTEXT("SellFailed", "Sell failed: {0}"), Friendly), EShopStatusSeverity::Error);
	}
	else
	{
		FString ItemName = ResolveItemNameFromInventory(Result.inventoryItemId);
		if (Result.quantity > 0 && Result.goldReceived == 0)
		{
			UE_LOG(LogGameplay, Warning, TEXT("VendorShop: sell success pays 0 (npc=%d invrow=%d qty=%d)"),
				CachedShop.npcId, Result.inventoryItemId, Result.quantity);
		}
		ShowStatus(FText::Format(LOCTEXT("SoldGain", "Sold {0}  (+{1} g)"),
			FText::FromString(ItemName), FText::AsNumber(Result.goldReceived)), EShopStatusSeverity::Success);
		TMap<FString, FString> ToastFields;
		ToastFields.Add(TEXT("amount"), FString::FromInt(Result.goldReceived));
		SendShopToast(TEXT("gold_received"), ToastFields);
	}
	RefreshInventoryDisplay();
}

void UVendorShopWidget::HandleBuyItemBatchResult(const FBuyItemBatchResultData& Result)
{
	if (Buy_Confirm_Button) Buy_Confirm_Button->SetIsEnabled(true);
	if (!Result.errorCode.IsEmpty())
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FText::Format(LOCTEXT("BuyFailed", "Buy failed: {0}"), Friendly), EShopStatusSeverity::Error);
		return;
	}
	// Echo-bound: stale/duplicate answers never touch the cart.
	if (!FVendorResultMatchRule::ShouldApply(PendingBuyBatchRequestId, Result.requestIdEcho, bBuyBatchApplied))
	{
		UE_LOG(LogGameplay, Warning, TEXT("VendorShop: stale buy batch result ignored (echo='%s')"),
			*Result.requestIdEcho);
		return;
	}
	bBuyBatchApplied = true;
	if (Result.items.Num() > 0 && Result.totalGoldSpent == 0)
	{
		UE_LOG(LogGameplay, Warning, TEXT("VendorShop: batch buy success spends 0 (npc=%d lines=%d)"),
			CachedShop.npcId, Result.items.Num());
	}
	FString FirstItemName;
	if (Result.items.Num() > 0)
		FirstItemName = ResolveItemNameFromShop(Result.items[0].itemId);
	if (Result.items.Num() <= 1)
		ShowStatus(FText::Format(LOCTEXT("BoughtSpent", "Bought {0}  (-{1} g)"),
			FText::FromString(FirstItemName), FText::AsNumber(Result.totalGoldSpent)), EShopStatusSeverity::Success);
	else
		ShowStatus(FText::Format(LOCTEXT("BoughtMoreSpent", "Bought {0} [+{1} more]  (-{2} g)"),
			FText::FromString(FirstItemName), FText::AsNumber(Result.items.Num() - 1), FText::AsNumber(Result.totalGoldSpent)), EShopStatusSeverity::Success);
	if (Result.items.Num() > 0)
	{
		TMap<FString, FString> ToastFields;
		ToastFields.Add(TEXT("itemId"), FString::FromInt(Result.items[0].itemId));
		ToastFields.Add(TEXT("itemSlug"), ResolveItemSlugFromShop(Result.items[0].itemId));
		ToastFields.Add(TEXT("quantity"), FString::FromInt(Result.items[0].quantity));
		SendShopToast(TEXT("item_received"), ToastFields);
	}
	BuyCart.Reset();
	RefreshBuyCartDisplay();
	RefreshShopDisplay(); // stock may have changed
}

void UVendorShopWidget::HandleSellItemBatchResult(const FSellItemBatchResultData& Result)
{
	if (Sell_Confirm_Button) Sell_Confirm_Button->SetIsEnabled(true);
	PendingSellExpected = 0;
	if (!Result.errorCode.IsEmpty())
	{
		const FText Friendly = GetVendorErrorText(Result.errorCode);
		ShowStatus(FText::Format(LOCTEXT("SellFailed", "Sell failed: {0}"), Friendly), EShopStatusSeverity::Error);
		RefreshSellCartDisplay();
		RefreshInventoryDisplay();
		return;
	}
	// Echo-bound: stale/duplicate answers never credit twice.
	if (!FVendorResultMatchRule::ShouldApply(PendingSellBatchRequestId, Result.requestIdEcho, bSellBatchApplied))
	{
		UE_LOG(LogGameplay, Warning, TEXT("VendorShop: stale sell batch result ignored (echo='%s')"),
			*Result.requestIdEcho);
		return;
	}
	bSellBatchApplied = true;
	if (Result.items.Num() > 0 && Result.totalGoldReceived == 0)
	{
		FString Dump;
		for (const FSellBatchItemResult& It : Result.items)
			Dump += FString::Printf(TEXT("[inv=%d qty=%d line=%d]"),
				It.inventoryItemId, It.quantity, It.goldReceived);
		UE_LOG(LogGameplay, Warning, TEXT("VendorShop: batch sell success pays 0 (npc=%d): %s"),
			CachedShop.npcId, *Dump);
	}
    FString FirstItemName;
    if (Result.items.Num() > 0)
        FirstItemName = ResolveItemNameFromInventory(Result.items[0].inventoryItemId);
    if (Result.items.Num() <= 1)
        ShowStatus(FText::Format(LOCTEXT("SoldGain", "Sold {0}  (+{1} g)"),
            FText::FromString(FirstItemName), FText::AsNumber(Result.totalGoldReceived)), EShopStatusSeverity::Success);
    else
        ShowStatus(FText::Format(LOCTEXT("SoldMoreGain", "Sold {0} [+{1} more]  (+{2} g)"),
            FText::FromString(FirstItemName), FText::AsNumber(Result.items.Num() - 1), FText::AsNumber(Result.totalGoldReceived)), EShopStatusSeverity::Success);
	TMap<FString, FString> ToastFields;
	ToastFields.Add(TEXT("amount"), FString::FromInt(Result.totalGoldReceived));
	SendShopToast(TEXT("gold_received"), ToastFields);

    // Server-authoritative gold: the result carries the absolute post-sale
    // balance — order-independent, idempotent by the echo match above.
    // Item rows converge via the absolute inventory snapshot
    // (HandleInventoryUpdated); the result never mutates rows, so a
    // snapshot-first arrival cannot double-remove.
    if (!Result.requestIdEcho.IsEmpty())
    {
        CachedInventory.gold = Result.newGoldBalance;
    }

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

void UVendorShopWidget::ShowStatus(const FText& Msg, EShopStatusSeverity Severity)
{
    if (Status_Text)
    {
        Status_Text->SetText(Msg);
        Status_Text->SetColorAndOpacity(FShopStatusStyle::ColorFor(Severity));
        Status_Text->SetVisibility(ESlateVisibility::Visible);
    }
    if (Severity == EShopStatusSeverity::Error)
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (GI->AudioManager)
                GI->AudioManager->PlayUISound(EUISoundEvent::ErrorSound);
        }
    }
}

#undef LOCTEXT_NAMESPACE

