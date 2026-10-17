#include "DevMode/DevModeBotBrain.h"
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

bool UDevModeBotBrain::Tick_VendorWalk(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Honest walk to the vendor (server validates range from its
        // stored position — teleports would fail the shop open).
        if (SteerToward(Player, VendorPos, 150.0f, Now, TEXT("vendor")))
        {
            Step = EBotStep::VendorSell;
            ShopLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at vendor npc=%d, opening shop"), VendorNpcId);
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
            if (Vend->GetCurrentShop().npcId == VendorNpcId)
            {
                bVendorShopOpened = true;
                BotAssert(true, TEXT("vendor-shop-open"), VendorNpcId, VendorNpcId);
                GoldBeforeSell = GetGold();
                SellInstanceId = FindSellableInstanceId(SellStartQty);
                if (SellInstanceId <= 0)
                {
                    Finish(false, TEXT("no-junk"));
                    return true;
                }
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT shop open (npc=%d) gold=%d, selling instance=%d qty=%d"),
                    VendorNpcId, GoldBeforeSell, SellInstanceId, SellStartQty);
            }
            else
            {
                if (!bShopRequestSent)
                {
                    Vend->RequestOpenVendorShop(CharId, VendorNpcId, Player->GetActorLocation());
                    bShopRequestSent = true;
                    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT vendor shop requested npc=%d"), VendorNpcId);
                }
                if (Now - ShopLegStartTime > ShopTimeoutSec)
                {
                    Finish(false, TEXT("shop-timeout"));
                }
            }
            return true;
        }
        if (!bSellRequested)
        {
            Vend->RequestSellItem(CharId, VendorNpcId, SellInstanceId, 1, Player->GetActorLocation());
            bSellRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT sell requested instance=%d"), SellInstanceId);
            return true;
        }
        // Server pushes the post-sale inventory snapshot (same packet the
        // shop UI reads) — gold up AND stack down by exactly one.
        if (GetGold() > GoldBeforeSell && GetQtyByInstance(SellInstanceId) == SellStartQty - 1)
        {
            BotAssert(true, TEXT("sell-gold-up"), 1, 1);
            BotAssert(true, TEXT("sell-qty-down"), 1, 1);
            BotShot(TEXT("sold"));
            Step = EBotStep::VendorRepair;
            ShopLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT sold: gold %d -> %d"), GoldBeforeSell, GetGold());
            return true;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            BotAssert(false, TEXT("sell-gold-up"), GetGold(), GoldBeforeSell + 1);
            Finish(false, TEXT("sell-timeout"));
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
            if (Rep->GetCurrentShop().npcId == VendorNpcId)
            {
                bRepairShopOpened = true;
                BotAssert(true, TEXT("repair-shop-open"), VendorNpcId, VendorNpcId);
                GoldBeforeRepair = GetGold();
                RepairInstanceId = 0;
                for (const FRepairShopItemData& Item : Rep->GetCurrentShop().items)
                {
                    if (Item.durabilityMax > 0 && Item.durabilityCurrent < Item.durabilityMax && Item.repairCost > 0)
                    {
                        RepairInstanceId = Item.inventoryItemId;
                        RepairMaxDur = Item.durabilityMax;
                        break;
                    }
                }
                if (RepairInstanceId <= 0)
                {
                    Finish(false, TEXT("no-damaged"));
                    return true;
                }
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repair shop open, target instance=%d maxdur=%d gold=%d"),
                    RepairInstanceId, RepairMaxDur, GoldBeforeRepair);
            }
            else
            {
                if (!bRepairShopRequestSent)
                {
                    Rep->RequestOpenRepairShop(CharId, VendorNpcId, Player->GetActorLocation());
                    bRepairShopRequestSent = true;
                    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repair shop requested npc=%d"), VendorNpcId);
                }
                if (Now - ShopLegStartTime > ShopTimeoutSec)
                {
                    Finish(false, TEXT("repair-shop-timeout"));
                }
            }
            return true;
        }
        if (!bRepairRequested)
        {
            Rep->RequestRepairItem(CharId, VendorNpcId, RepairInstanceId, Player->GetActorLocation());
            bRepairRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repair requested instance=%d"), RepairInstanceId);
            return true;
        }
        // The repaired row drops OUT of the shop list (only damaged rows
        // are listed), so durability is read from the server inventory
        // snapshot + gold must have dropped by the repair cost.
        const int32 NowDur = GetDurabilityByInstance(RepairInstanceId);
        if (NowDur == RepairMaxDur && GetGold() < GoldBeforeRepair)
        {
            BotAssert(true, TEXT("repair-durability-max"), NowDur, RepairMaxDur);
            BotAssert(GetGold() < GoldBeforeRepair, TEXT("repair-gold-down"), GetGold(), GoldBeforeRepair);
            BotShot(TEXT("repaired"));
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repaired: durability %d, gold %d -> %d"),
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
