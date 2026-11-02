#include "UI/TradeWidget.h"
#include "UI/QuantityPopupWidget.h"
#include "Prototyping.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationStackWidget.h"
#include "Audio/AudioManager.h"
#include "Utils/NetCore/TradeStatusRule.h"
#include "Utils/NetCore/InventoryQuery.h"
#include "Gameplay/Items/InventoryManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

// S5b: trade chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "Trade"

void UTradeWidget::DispatchAddItem(int32 InventoryItemId, int32 ItemId, int32 Quantity)
{
    if (!TradeManager || !TradeManager->IsInTrade()) return;

    // Owned total caps the entry: the server validates per entry, so two
    // entries for one stack could pass validation but overdraw on execute.
    int32 OwnedQty = 0;
    for (const FInventoryItemStruct& Item : CachedInventory.items)
    {
        if (Item.id == InventoryItemId) { OwnedQty = Item.quantity; break; }
    }
    if (OwnedQty <= 0) return;
    Quantity = FMath::Clamp(Quantity, 1, OwnedQty);

    // Merge into the existing entry for this stack instead of duplicating it.
    for (FTradeOfferItem& Existing : PendingOffer)
    {
        if (Existing.inventoryItemId == InventoryItemId)
        {
            Existing.quantity = FMath::Min(Existing.quantity + Quantity, OwnedQty);
            PendingGold = Gold_Input ? FCString::Atoi(*Gold_Input->GetText().ToString()) : PendingGold;
            const int32 MergeCharId = GetGameInstance()
                ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
            bOwnOfferPending = true;
            TradeManager->UpdateTradeOffer(MergeCharId, PendingGold, PendingOffer);
            return;
        }
    }

    FTradeOfferItem NewItem;
    NewItem.inventoryItemId = InventoryItemId;
    NewItem.itemId          = ItemId;
    NewItem.quantity        = Quantity;
    PendingOffer.Add(NewItem);

    // Resolve gold from input field
    if (Gold_Input)
    {
        PendingGold = FCString::Atoi(*Gold_Input->GetText().ToString());
    }

    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    bOwnOfferPending = true;
    TradeManager->UpdateTradeOffer(CharId, PendingGold, PendingOffer);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UTradeWidget::RebuildOfferList(UPanelWidget* Box, TArray<UVendorSlotWidget*>& OutSlots, const TArray<FInventoryItemStruct>& Items, bool bIsMyOffer)
{
    OutSlots.Reset();
    if (!Box || !TradeSlotClass)
    {
        if (!TradeSlotClass)
            UE_LOG(LogGameplay, Warning, TEXT("TradeWidget: TradeSlotClass is not set - offer list is empty (assign the vendor slot BP)"));
        return;
    }
    Box->ClearChildren();

    int32 SlotIndex = 0;
    for (const FInventoryItemStruct& Item : Items)
    {
        if (Item.itemId <= 0 || Item.quantity <= 0) continue;
        UVendorSlotWidget* OfferSlot = CreateWidget<UVendorSlotWidget>(GetOwningPlayer(), TradeSlotClass);
        if (!OfferSlot) continue;

        FText ItemName = FText::FromString(Item.slug);
        if (!Item.slug.IsEmpty())
        {
            if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
            {
                if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                {
                    const FText LocName = Loc->GetItemDisplayName(Item.slug);
                    if (!LocName.IsEmpty()) ItemName = LocName;
                }
            }
        }

        FVendorCartEntry Entry;
        Entry.itemId = Item.itemId;
        Entry.inventoryItemId = Item.id;
        Entry.itemSlug = Item.slug;
        Entry.itemName = ItemName.ToString();
        Entry.slug = Item.slug;
        Entry.quantity = Item.quantity;
        Entry.maxQuantity = Item.quantity;
        Entry.pricePerUnit = 0; // trade has no prices; hidden via SetShowPrice
        OfferSlot->SetupAsCartSlot(SlotIndex, Entry);
        // Full source item: cart tooltip/rarity match inventory (no
        // minimal-preview lies about rarity/stats).
        OfferSlot->SetFullItemData(Item);
        // My offer: [X] removes the stack back (resend without it).
        // Their offer: display only.
        OfferSlot->SetAllowRemoveButton(bIsMyOffer);
        OfferSlot->SetShowPrice(false);
        if (bIsMyOffer)
        {
            OfferSlot->OnRemoveFromCart.AddDynamic(this, &UTradeWidget::HandleMyOfferRemove);
            OfferSlot->OnVendorSlotClicked.AddDynamic(this, &UTradeWidget::HandleMyOfferClicked);
            OfferSlot->OnVendorSlotHovered.AddDynamic(this, &UTradeWidget::HandleMyOfferSlotHovered);
        }
        else
        {
            OfferSlot->OnVendorSlotHovered.AddDynamic(this, &UTradeWidget::HandleTheirOfferSlotHovered);
        }
        OutSlots.Add(OfferSlot);
        Box->AddChild(OfferSlot);
        ++SlotIndex;
    }
}

void UTradeWidget::HandleMyOfferSlotHovered(int32 SlotIndex, bool bIsHovered)
{
    ShowOfferTooltip(MyOfferSlots, SlotIndex, bIsHovered);
}

void UTradeWidget::HandleTheirOfferSlotHovered(int32 SlotIndex, bool bIsHovered)
{
    ShowOfferTooltip(TheirOfferSlots, SlotIndex, bIsHovered);
}

void UTradeWidget::HandleMyOfferRemove(int32 SlotIndex)
{
    if (!TradeManager || !TradeManager->IsInTrade()) return;
    if (!MyOfferSlots.IsValidIndex(SlotIndex) || !MyOfferSlots[SlotIndex]) return;
    // Drop the [X]-clicked stack from the pending offer (unacked adds
    // survive: removal edits intent, errors roll back to accepted).
    HandleMyOfferQuantityRemove(MyOfferSlots[SlotIndex]->GetCartEntryData().inventoryItemId);
}

void UTradeWidget::HandleMyOfferClicked(int32 SlotIndex)
{
    if (!TradeManager || !TradeManager->IsInTrade()) return;
    if (!MyOfferSlots.IsValidIndex(SlotIndex) || !MyOfferSlots[SlotIndex]) return;
    if (!TradeQuantityPopupClass) return;
    const int32 DropId = MyOfferSlots[SlotIndex]->GetCartEntryData().inventoryItemId;
    const FInventoryItemStruct* Owned = nullptr;
    for (const FInventoryItemStruct& Item : CachedInventory.items)
    {
        if (Item.id == DropId) { Owned = &Item; break; }
    }
    if (!Owned || Owned->id <= 0) return;
    int32 OfferedQty = 0;
    for (const FTradeOfferItem& Entry : PendingOffer)
    {
        if (Entry.inventoryItemId == DropId) { OfferedQty = Entry.quantity; break; }
    }
    if (OfferedQty <= 0) OfferedQty = 1;
    EnsureTradeQuantityPopup();
    if (!TradeQuantityPopup) return;
    // Shared popup instance: clear both delegates (add path never binds
    // Remove; a stale binding would fire a foreign removal).
    TradeQuantityPopup->OnQuantityConfirmed.Clear();
    TradeQuantityPopup->OnQuantityRemove.Clear();
    TradeQuantityPopup->OpenForUpdate(DropId, GetTradeDisplayName(*Owned).ToString(), 0, Owned->quantity, OfferedQty);
    TradeQuantityPopup->OnQuantityConfirmed.AddDynamic(this, &UTradeWidget::HandleMyOfferQuantityConfirmed);
    TradeQuantityPopup->OnQuantityRemove.AddDynamic(this, &UTradeWidget::HandleMyOfferQuantityRemove);
}

void UTradeWidget::HandleMyOfferQuantityConfirmed(int32 InventoryItemId, int32 Quantity)
{
    if (!TradeManager || !TradeManager->IsInTrade()) return;
    if (InventoryItemId <= 0) return;
    int32 OwnedQty = 0;
    for (const FInventoryItemStruct& Item : CachedInventory.items)
    {
        if (Item.id == InventoryItemId) { OwnedQty = Item.quantity; break; }
    }
    if (OwnedQty <= 0) return;
    const int32 Clamped = FMath::Clamp(Quantity, 1, OwnedQty);
    bool bFound = false;
    for (FTradeOfferItem& Entry : PendingOffer)
    {
        if (Entry.inventoryItemId == InventoryItemId)
        {
            Entry.quantity = Clamped;
            bFound = true;
            break;
        }
    }
    if (!bFound) return;
    if (Gold_Input)
        PendingGold = FMath::Max(0, FCString::Atoi(*Gold_Input->GetText().ToString()));
    bOwnOfferPending = true;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    TradeManager->UpdateTradeOffer(CharId, PendingGold, PendingOffer);
}

void UTradeWidget::HandleMyOfferQuantityRemove(int32 InventoryItemId)
{
    if (!TradeManager || !TradeManager->IsInTrade()) return;
    if (InventoryItemId <= 0) return;
    TArray<FTradeOfferItem> Kept;
    Kept.Reserve(PendingOffer.Num());
    for (const FTradeOfferItem& Entry : PendingOffer)
    {
        if (Entry.inventoryItemId != InventoryItemId && Entry.inventoryItemId > 0)
            Kept.Add(Entry);
    }
    PendingOffer = MoveTemp(Kept);
    if (Gold_Input)
        PendingGold = FMath::Max(0, FCString::Atoi(*Gold_Input->GetText().ToString()));
    bOwnOfferPending = true;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    TradeManager->UpdateTradeOffer(CharId, PendingGold, PendingOffer);
}

FText UTradeWidget::GetTradeDisplayName(const FInventoryItemStruct& Item) const
{
    FText Name = FText::FromString(!Item.name.IsEmpty() ? Item.name : Item.slug);
    if (!Item.slug.IsEmpty())
    {
        if (const UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (const ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                const FText LocName = Loc->GetItemDisplayName(Item.slug);
                if (!LocName.IsEmpty())
                    Name = LocName;
            }
        }
    }
    return Name;
}

void UTradeWidget::ShowOfferTooltip(const TArray<UVendorSlotWidget*>& Slots, int32 SlotIndex, bool bIsHovered)
{
    // Tooltips read the slot's own cart preview (slug + quantity are enough).
    if (!TradeTooltipWidgetClass) return;
    EnsureTradeTooltip();
    if (!TradeTooltip) return;
    if (bIsHovered && Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex])
    {
        TradeTooltip->SetItemData(Slots[SlotIndex]->GetTooltipData());
        FVector2D Mouse = FVector2D::ZeroVector;
        if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
            PC->GetMousePosition(Mouse.X, Mouse.Y);
        TradeTooltip->UpdateTooltipPosition(Mouse);
        TradeTooltip->ShowTooltip();
    }
    else
    {
        TradeTooltip->HideTooltip();
    }
}

void UTradeWidget::RebuildInventoryList()
{
    if (!Inv_Items_Box) return;

    Inv_Items_Box->ClearChildren();
    TradeInvSlots.Reset();

    if (!TradeSlotClass)
    {
        // Loud skip (was silent): without the slot BP there is nothing to show.
        UE_LOG(LogGameplay, Warning, TEXT("TradeWidget: TradeSlotClass is not set - inventory pick list is empty (assign the vendor slot BP)"));
        return;
    }
    if (CachedInventory.items.Num() == 0)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("TradeWidget: inventory cache is empty - nothing to offer"));
        return;
    }

    int32 SlotIndex = 0;
    for (const FInventoryItemStruct& Item : CachedInventory.items)
    {
        if (Item.itemId <= 0) continue;
        // Wallet (110): currency travels via the gold field, never as rows.
        if (FInventoryQuery::IsCurrencyItem(Item)) continue;
        // Equipped gear can never be offered (server rejects it, like the
        // vendor sell list skips it) — hide it instead of showing dead rows.
        if (Item.is_equipped) continue;
        UVendorSlotWidget* InvSlot = CreateWidget<UVendorSlotWidget>(GetOwningPlayer(), TradeSlotClass);
        if (!InvSlot) continue;

        // Show remaining qty after the current offer (badge), checkmark the
        // rows already offered. Sort/filter state lives in the inventory
        // widget; the trade list mirrors the raw snapshot order.
        int32 OfferedQty = 0;
        bool bInOffer = false;
        for (const FTradeOfferItem& Offered : PendingOffer)
        {
            if (Offered.inventoryItemId == Item.id)
            {
                bInOffer = true;
                OfferedQty += Offered.quantity;
                break;
            }
        }
        // Fully offered stacks go dark (no dead clicks): the popup max is
        // the remainder, and DispatchAddItem caps at owned anyway.
        const bool bFullyOffered = OfferedQty >= Item.quantity;
        InvSlot->SetupAsInvSlot(SlotIndex, Item, bInOffer, bFullyOffered, OfferedQty);
        InvSlot->SetAllowRemoveButton(false);
        InvSlot->OnVendorSlotClicked.AddDynamic(this, &UTradeWidget::HandleTradeInvSlotClicked);
        InvSlot->OnVendorSlotHovered.AddDynamic(this, &UTradeWidget::HandleTradeInvSlotHovered);
        TradeInvSlots.Add(InvSlot);
        Inv_Items_Box->AddChild(InvSlot);
        ++SlotIndex;
    }
}

void UTradeWidget::HandleTradeInvSlotClicked(int32 SlotIndex)
{
    if (!TradeInvSlots.IsValidIndex(SlotIndex) || !TradeInvSlots[SlotIndex]) return;
    const FInventoryItemStruct Item = TradeInvSlots[SlotIndex]->GetInvItemData();
    if (Item.id <= 0) return;
    int32 OfferedQty = 0;
    for (const FTradeOfferItem& Offered : PendingOffer)
    {
        if (Offered.inventoryItemId == Item.id) { OfferedQty += Offered.quantity; break; }
    }
    const int32 Remaining = Item.quantity - OfferedQty;
    if (Remaining <= 0) return; // fully offered — row is dark, no dead clicks
    // Single pieces go straight in; stacks ask for a quantity first.
    if (Item.quantity <= 1 || !TradeQuantityPopupClass)
    {
        DispatchAddItem(Item.id, Item.itemId, 1);
        return;
    }
    EnsureTradeQuantityPopup();
    if (!TradeQuantityPopup) { DispatchAddItem(Item.id, Item.itemId, 1); return; }
    FText ItemName = GetTradeDisplayName(Item);
    TradeQuantityPopup->OnQuantityConfirmed.Clear();
    TradeQuantityPopup->OnQuantityRemove.Clear();
    TradeQuantityPopup->OpenForAdd(Item.id, ItemName.ToString(), 0, Remaining, 1);
    TradeQuantityPopup->OnQuantityConfirmed.AddDynamic(this, &UTradeWidget::HandleTradeQuantityConfirmed);
}

void UTradeWidget::HandleTradeQuantityConfirmed(int32 SlotIndex, int32 Quantity)
{
    // SlotIndex carries the inventoryItemId (see OpenForAdd above).
    for (const FInventoryItemStruct& Item : CachedInventory.items)
    {
        if (Item.id == SlotIndex)
        {
            DispatchAddItem(Item.id, Item.itemId, Quantity);
            return;
        }
    }
}

void UTradeWidget::EnsureTradeQuantityPopup()
{
    if (TradeQuantityPopup || !TradeQuantityPopupClass || !GetWorld()) return;
    TradeQuantityPopup = CreateWidget<UQuantityPopupWidget>(GetOwningPlayer(), TradeQuantityPopupClass);
    if (TradeQuantityPopup)
    {
        TradeQuantityPopup->SetShowPricing(false);
        TradeQuantityPopup->AddToViewport(999);
        TradeQuantityPopup->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UTradeWidget::HandleTradeInvSlotHovered(int32 SlotIndex, bool bIsHovered)
{
    if (!TradeTooltipWidgetClass) return;
    if (bIsHovered && TradeInvSlots.IsValidIndex(SlotIndex) && TradeInvSlots[SlotIndex])
    {
        EnsureTradeTooltip();
        if (!TradeTooltip) return;
        TradeTooltip->SetItemData(TradeInvSlots[SlotIndex]->GetInvItemData());
        FVector2D Mouse = FVector2D::ZeroVector;
        if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
            PC->GetMousePosition(Mouse.X, Mouse.Y);
        TradeTooltip->UpdateTooltipPosition(Mouse);
        TradeTooltip->ShowTooltip();
    }
    else if (TradeTooltip)
    {
        TradeTooltip->HideTooltip();
    }
}

void UTradeWidget::EnsureTradeTooltip()
{
    if (TradeTooltip || !TradeTooltipWidgetClass || !GetWorld()) return;
    TradeTooltip = CreateWidget<UItemTooltipWidget>(GetWorld(), TradeTooltipWidgetClass);
    if (TradeTooltip)
    {
        TradeTooltip->AddToViewport(1000);
        TradeTooltip->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UTradeWidget::HandleLocaleChanged(const FString& NewLocale)
{
    if (GetVisibility() == ESlateVisibility::Visible)
    {
        RefreshTradeState();
    }
}

void UTradeWidget::HandleGoldTextChanged(const FText& Text)
{
    FString Str = Text.ToString();
    FString Filtered;
    for (const TCHAR Ch : Str)
    {
        if (FChar::IsDigit(Ch))
            Filtered.AppendChar(Ch);
    }
    if (Filtered != Str)
    {
        Gold_Input->SetText(FText::FromString(Filtered));
    }
}

void UTradeWidget::HandleGoldTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    // Typing alone never sends: only an explicit commit (Enter / unfocus)
    // proposes the gold offer — the peer sees it, both locks drop.
    if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus) return;
    if (!TradeManager || !TradeManager->IsInTrade() || !Gold_Input) return;

    const int32 Want = FMath::Max(0, FCString::Atoi(*Text.ToString()));
    const int32 Balance = CachedInventory.gold;
    const int32 Clamped = FTradeStatusRule::ClampGoldOffer(Balance, Want);
    if (!FTradeStatusRule::CanAffordGold(Balance, Want))
    {
        ShowTradeHint(
            TEXT("Not enough gold"),
            FString::Printf(TEXT("You have %d g — offer clamped to your balance."), Balance));
        Gold_Input->SetText(FText::FromString(FString::FromInt(Clamped)));
    }
    PendingGold = Clamped;
    // No-op when nothing changed since the accepted state (avoids empty spam).
    if (!bOwnOfferPending && Clamped == CurrentState.myGold) return;
    bOwnOfferPending = true;
    const int32 CharId = GetGameInstance()
        ? Cast<UMyGameInstance>(GetGameInstance())->GetCurrentCharacterID() : 0;
    TradeManager->UpdateTradeOffer(CharId, PendingGold, PendingOffer);
}

#undef LOCTEXT_NAMESPACE
