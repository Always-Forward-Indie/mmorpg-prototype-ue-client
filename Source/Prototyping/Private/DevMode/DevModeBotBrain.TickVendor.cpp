#include "DevMode/DevModeBotBrain.h"
#include "Prototyping.h"
#include "DevMode/DevModeConsoleCommands.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/TitleNetworkHandler.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Services/LocalizationSubsystem.h"
#include "Audio/AudioManager.h"
#include "Authentication/AuthenticationManager.h"
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "UI/UIManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HighResScreenshot.h"
#include "Misc/Parse.h"
#include "DevMode/BotBrainHelpers.h"
#include "Utils/NetCore/VendorSyncRule.h"

bool UDevModeBotBrain::Tick_VendorWalk(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Honest walk to the vendor (server validates range from its
        // stored position — teleports would fail the shop open).
        if (SteerToward(Player, VendorPos, 150.0f, Now, TEXT("vendor")))
        {
            Step = EBotStep::VendorSell;
            ShopLegStartTime = Now;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT at vendor npc=%d, opening shop"), VendorNpcId);
            BotShot(TEXT("vendor"));
            return true;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            Finish(false, TEXT("vendor-unreachable"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_VendorSell(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        UVendorManager* Vend = GameInstance->GetVendorManager();
        UInventoryManager* Inv = GameInstance->GetInventoryManager();
        if (!Vend || !Inv)
        {
            Finish(false, TEXT("no-vendor"));
            return true;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        if (!bVendorShopOpened)
        {
            // Open handshake owned by FVendorSyncRule (pure, spec-pinned);
            // run flags and timers stay here.
            switch (FVendorSyncRule::NextOpenStep(Vend->GetCurrentShop().npcId, VendorNpcId, bShopRequestSent))
            {
            case FVendorSyncRule::EOpenStep::ShopOpen:
            {
                bVendorShopOpened = true;
                BotAssert(true, TEXT("vendor-shop-open"), VendorNpcId, VendorNpcId);
                GoldBeforeSell = GetGold();
                SellInstanceId = FindSellableInstanceId(SellStartQty);
                if (SellInstanceId <= 0)
                {
                    // Empty-handed is an account state, not a bot failure:
                    // skip the sell leg, the buy leg still proves commerce.
                    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT no junk to sell, skipping sell leg"));
                    Step = EBotStep::VendorBuy;
                    ShopLegStartTime = Now;
                    return true;
                }
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT shop open (npc=%d) gold=%d, selling instance=%d qty=%d"),
                    VendorNpcId, GoldBeforeSell, SellInstanceId, SellStartQty);
                break;
            }
            case FVendorSyncRule::EOpenStep::SendOpenRequest:
            {
                Vend->RequestOpenVendorShop(CharId, VendorNpcId, Player->GetActorLocation());
                bShopRequestSent = true;
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT vendor shop requested npc=%d"), VendorNpcId);
                break;
            }
            case FVendorSyncRule::EOpenStep::AwaitOpen:
            default:
                break;
            }
            if (!bVendorShopOpened && Now - ShopLegStartTime > ShopTimeoutSec)
            {
                Finish(false, TEXT("shop-timeout"));
            }
            return true;
        }
        if (bVendorNegativePhase)
        {
            // Negative run: no selling (it would fund the buy under test).
            Step = EBotStep::VendorBuy;
            ShopLegStartTime = Now;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT negative mode: skipping sell leg"));
            return true;
        }
        if (!bSellRequested)
        {
            Vend->RequestSellItem(CharId, VendorNpcId, SellInstanceId, 1, Player->GetActorLocation());
            bSellRequested = true;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT sell requested instance=%d"), SellInstanceId);
            return true;
        }
        // Server pushes the post-sale inventory snapshot (same packet the
        // shop UI reads) — gold up AND stack down by exactly one.
        if (FVendorSyncRule::IsSellSynced(GetGold(), GoldBeforeSell, GetQtyByInstance(SellInstanceId), SellStartQty))
        {
            BotAssert(true, TEXT("sell-gold-up"), 1, 1);
            BotAssert(true, TEXT("sell-qty-down"), 1, 1);
            BotShot(TEXT("sold"));
            Step = EBotStep::VendorBuy;
            ShopLegStartTime = Now;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT sold: gold %d -> %d"), GoldBeforeSell, GetGold());
            return true;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            BotAssert(false, TEXT("sell-gold-up"), GetGold(), GoldBeforeSell + 1);
            Finish(false, TEXT("sell-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_VendorBuy(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
    UVendorManager* Vend = GameInstance->GetVendorManager();
    UInventoryManager* Inv = GameInstance->GetInventoryManager();
    if (!Vend || !Inv)
    {
        Finish(false, TEXT("no-vendor"));
        return true;
    }
    const int32 CharId = GameInstance->GetCurrentCharacterID();
    auto SumQtyByItemId = [Inv](int32 ItemId)
    {
        int32 Total = 0;
        for (const FInventoryItemStruct& Item : Inv->GetInventory().items)
        {
            if (Item.itemId == ItemId) { Total += Item.quantity; }
        }
        return Total;
    };
    if (!bBuyRequested)
    {
        // Cheapest affordable row from the still-open shop (same snapshot
        // the shop UI reads). Predicate lives in FVendorSyncRule.
        TArray<FVendorSyncRule::FBuyableItem> Goods;
        for (const FVendorShopItemData& Item : Vend->GetCurrentShop().items)
        {
            FVendorSyncRule::FBuyableItem G;
            G.ItemId = Item.itemId;
            G.PriceBuy = Item.priceBuy;
            Goods.Add(G);
        }
        GoldBeforeBuy = GetGold();
        if (bVendorNegativePhase && !bNegativeBuyRequested)
        {
            // S12 refusal probe: gold is 0 by pre-run setup, so even the
            // cheapest row is unaffordable. Request it anyway — the server
            // must refuse (error packet) and change nothing.
            int32 CheapestIdx = INDEX_NONE;
            int32 CheapestPrice = TNumericLimits<int32>::Max();
            for (int32 i = 0; i < Goods.Num(); ++i)
            {
                if (Goods[i].PriceBuy < CheapestPrice)
                {
                    CheapestPrice = Goods[i].PriceBuy;
                    CheapestIdx = i;
                }
            }
            if (CheapestIdx == INDEX_NONE)
            {
                Finish(false, TEXT("negative-empty-shop"));
                return true;
            }
            NegativeBuyItemId = Goods[CheapestIdx].ItemId;
            NegativeBuyQtyBefore = SumQtyByItemId(NegativeBuyItemId);
            Vend->RequestBuyItem(CharId, VendorNpcId, NegativeBuyItemId, 1, Player->GetActorLocation());
            bNegativeBuyRequested = true;
            NegativeBuyRequestTime = Now;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT negative buy requested item=%d (gold=%d, price=%d) — expecting refusal"),
                NegativeBuyItemId, GoldBeforeBuy, CheapestPrice);
            return true;
        }
        if (bVendorNegativePhase)
        {
            // Bought despite zero gold = server wrongly synced: loud FAIL.
            if (SumQtyByItemId(NegativeBuyItemId) > NegativeBuyQtyBefore)
            {
                BotAssert(false, TEXT("negative-buy-refused"), GetGold(), GoldBeforeBuy);
                Finish(false, TEXT("negative-buy-wrongly-synced"));
                return true;
            }
            if (GetGold() == 0 && Now - NegativeBuyRequestTime > 20.0)
            {
                BotAssert(true, TEXT("negative-buy-refused"), 1, 1);
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT negative buy refused as expected (gold still 0, qty unchanged)"));
                Finish(true, TEXT("negative-proven"));
                return true;
            }
            if (Now - ShopLegStartTime > ShopTimeoutSec)
            {
                BotAssert(false, TEXT("negative-buy-refused"), GetGold(), 0);
                Finish(false, TEXT("negative-timeout"));
            }
            return true;
        }
        const int32 BuyIdx = FVendorSyncRule::FindCheapestAffordableIndex(Goods, GoldBeforeBuy);
        if (BuyIdx == INDEX_NONE)
        {
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT nothing affordable (gold=%d), skipping buy"), GoldBeforeBuy);
            Step = EBotStep::VendorRepair;
            ShopLegStartTime = Now;
            return true;
        }
        BuyItemId = Goods[BuyIdx].ItemId;
        BuyPrice = Goods[BuyIdx].PriceBuy;
        BuyQtyBefore = SumQtyByItemId(BuyItemId);
        Vend->RequestBuyItem(CharId, VendorNpcId, BuyItemId, 1, Player->GetActorLocation());
        bBuyRequested = true;
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT buy requested item=%d price=%d"), BuyItemId, BuyPrice);
        return true;
    }
    // Server snapshot confirms the purchase: gold dropped by exactly the
    // price AND the stack grew by one.
    if (FVendorSyncRule::IsBuySynced(GetGold(), GoldBeforeBuy, BuyPrice, SumQtyByItemId(BuyItemId), BuyQtyBefore))
    {
        BotAssert(true, TEXT("buy-gold-down"), 1, 1);
        BotAssert(true, TEXT("buy-qty-up"), 1, 1);
        BotShot(TEXT("bought"));
        Step = EBotStep::VendorRepair;
        ShopLegStartTime = Now;
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT bought item=%d for %d gold"), BuyItemId, BuyPrice);
        return true;
    }
    if (Now - ShopLegStartTime > ShopTimeoutSec)
    {
        BotAssert(false, TEXT("buy-gold-down"), GetGold(), GoldBeforeBuy - BuyPrice);
        Finish(false, TEXT("buy-timeout"));
    }
    return true;
}

bool UDevModeBotBrain::Tick_VendorRepair(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        URepairManager* Rep = GameInstance->GetRepairManager();
        if (!Rep)
        {
            Finish(false, TEXT("no-repair"));
            return true;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        if (!bRepairShopOpened)
        {
            // Same open handshake as the vendor leg, one rule for both.
            switch (FVendorSyncRule::NextOpenStep(Rep->GetCurrentShop().npcId, VendorNpcId, bRepairShopRequestSent))
            {
            case FVendorSyncRule::EOpenStep::ShopOpen:
            {
                bRepairShopOpened = true;
                BotAssert(true, TEXT("repair-shop-open"), VendorNpcId, VendorNpcId);
                GoldBeforeRepair = GetGold();
                TArray<FVendorSyncRule::FRepairableItem> Rows;
                for (const FRepairShopItemData& Item : Rep->GetCurrentShop().items)
                {
                    FVendorSyncRule::FRepairableItem R;
                    R.InstanceId = Item.inventoryItemId;
                    R.DurabilityMax = Item.durabilityMax;
                    R.DurabilityCurrent = Item.durabilityCurrent;
                    R.RepairCost = Item.repairCost;
                    Rows.Add(R);
                }
                RepairInstanceId = 0;
                const int32 RepairableIdx = FVendorSyncRule::FindRepairableIndex(Rows);
                if (RepairableIdx != INDEX_NONE)
                {
                    RepairInstanceId = Rows[RepairableIdx].InstanceId;
                    RepairMaxDur = Rows[RepairableIdx].DurabilityMax;
                }
                if (RepairInstanceId <= 0)
                {
                    Finish(false, TEXT("no-damaged"));
                    return true;
                }
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT repair shop open, target instance=%d maxdur=%d gold=%d"),
                    RepairInstanceId, RepairMaxDur, GoldBeforeRepair);
                break;
            }
            case FVendorSyncRule::EOpenStep::SendOpenRequest:
            {
                Rep->RequestOpenRepairShop(CharId, VendorNpcId, Player->GetActorLocation());
                bRepairShopRequestSent = true;
                UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT repair shop requested npc=%d"), VendorNpcId);
                break;
            }
            case FVendorSyncRule::EOpenStep::AwaitOpen:
            default:
                break;
            }
            if (!bRepairShopOpened && Now - ShopLegStartTime > ShopTimeoutSec)
            {
                Finish(false, TEXT("repair-shop-timeout"));
            }
            return true;
        }
        if (!bRepairRequested)
        {
            Rep->RequestRepairItem(CharId, VendorNpcId, RepairInstanceId, Player->GetActorLocation());
            bRepairRequested = true;
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT repair requested instance=%d"), RepairInstanceId);
            return true;
        }
        // The repaired row drops OUT of the shop list (only damaged rows
        // are listed), so durability is read from the server inventory
        // snapshot + gold must have dropped by the repair cost.
        const int32 NowDur = GetDurabilityByInstance(RepairInstanceId);
        if (FVendorSyncRule::IsRepairSynced(NowDur, RepairMaxDur, GetGold(), GoldBeforeRepair))
        {
            BotAssert(true, TEXT("repair-durability-max"), NowDur, RepairMaxDur);
            BotAssert(GetGold() < GoldBeforeRepair, TEXT("repair-gold-down"), GetGold(), GoldBeforeRepair);
            BotShot(TEXT("repaired"));
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT repaired: durability %d, gold %d -> %d"),
                NowDur, GoldBeforeRepair, GetGold());
            Finish(Failures == 0, Failures == 0 ? TEXT("vendor-done") : TEXT("assert-failures"));
            return true;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            BotAssert(false, TEXT("repair-durability-max"), NowDur, RepairMaxDur);
            Finish(false, TEXT("repair-timeout"));
        }
        return true;
}
