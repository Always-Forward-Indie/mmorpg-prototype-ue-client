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

bool UDevModeBotBrain::Tick_GotoRendezvous(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Walk to the meeting point (server-tick honest pace via SteerToward),
        // then wait for the peer's remote actor. Arrival radius is generous:
        // visibility interest ranges are wide, precision doesn't matter.
        if (SteerToward(Player, RendezvousPoint, 150.0f, Now, TEXT("rendezvous")))
        {
            Step = EBotStep::AssertVisible;
            StepStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at rendezvous, waiting for peer"));
            BotShot(TEXT("rendezvous"));
            return true;
        }
        if (Now - StepStartTime > RendezvousTimeoutSec)
        {
            Finish(false, TEXT("rendezvous-timeout"));
        }
        return true;
}


bool UDevModeBotBrain::Tick_AssertVisible(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // A remote player is visible iff SpawnedPlayers holds a live actor
        // under someone else's clientId (this client owns exactly one entry).
        // Assert exactly once (on success or timeout), not per poll.
        const int32 OwnId = GameInstance->GetCurrentClientID();
        bool bPeerVisible = false;
        for (const TPair<int32, ABasicPlayer*>& E : GameInstance->SpawnedPlayers)
        {
            if (E.Key != OwnId && IsValid(E.Value) && !E.Value->IsActorBeingDestroyed())
            {
                bPeerVisible = true;
                break;
            }
        }
        if (bPeerVisible)
        {
            BotAssert(true, TEXT("peer-visible"), 1, 1);
            if (bChatPhase)
            {
                // Social leg: stay at the rendezvous and talk next.
                Step = EBotStep::ChatExchange;
                ChatLegStartTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT peer visible, starting chat exchange"));
                return true;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("peer-visible") : TEXT("assert-failures"));
            return true;
        }
        if (Now - StepStartTime > RendezvousTimeoutSec)
        {
            BotAssert(false, TEXT("peer-visible"), 0, 1);
            Finish(false, TEXT("peer-timeout"));
        }
        return true;
}


bool UDevModeBotBrain::Tick_GotoCorpse(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Walk back to the last kill (same steering/odometer proof as every
        // other leg). Arrival radius is generous: the drop spawns at the
        // corpse and pickup interest is wide.
        if (SteerToward(Player, LastKillLocation, 150.0f, Now, TEXT("corpse")))
        {
            Step = EBotStep::LootPickup;
            StepStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at corpse, resolving pickup"));
            BotShot(TEXT("corpse"));
            return true;
        }
        if (Now - LootLegStartTime > LootTimeoutSec)
        {
            Finish(false, TEXT("corpse-timeout"));
        }
        return true;
}


bool UDevModeBotBrain::Tick_LootPickup(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        if (bStubLoot)
        {
            // Offline: no server, so the drop itself is stubbed — through the
            // real SpawnDroppedItem path — and resolved through the real
            // inventory/pickup calls. Walking here and the asserts are real.
            if (!LootDropActor.IsValid() && !bPickupRequested)
            {
                if (!SpawnStubDrop(Player))
                {
                    Finish(false, TEXT("stub-drop-failed"));
                    return true;
                }
            }
            if (bStubHarvest)
            {
                // Offline: no harvest progress server exists. The completion
                // is stubbed through the real SetHarvested path (same flag
                // the server-driven harvest sets) before the stub pickup.
                if (ABasicMOB* Corpse = ResolveCorpseActor())
                {
                    if (!Corpse->HasBeenHarvested())
                    {
                        Corpse->SetHarvested(true);
                        UE_LOG(LogTemp, Warning, TEXT("%s_HARVEST_STUB uid=%d marked harvested (offline progress)"),
                            *TombstonePrefix, CorpseUID);
                    }
                }
            }
            ResolveStubLoot(Player);
            const int32 Have = CountInventoryItems();
            const int32 HaveQty = SumInventoryQuantities();
            BotAssert(Have == InitialInvCount + 1, TEXT("loot-inv-plus-one"), Have, InitialInvCount + 1);
            BotAssert(HaveQty == InitialInvQty + 1, TEXT("loot-qty-plus-one"), HaveQty, InitialInvQty + 1);
            BotShot(TEXT("loot"));
            if (bQuestPhase)
            {
                QuestLootDone(Now);
                return true;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
            return true;
        }
        // Live: the real game flow — harvest the victim's corpse, inspect
        // its loot, pick it all up. (World itemDrop packets on the dev
        // стенд carry empty item lists; corpse loot is the loot path.)
        UHarvestManager* Harvest = GameInstance->GetHarvestManager();
        if (!Harvest)
        {
            Finish(false, TEXT("no-harvest-manager"));
            return true;
        }
        ABasicMOB* Corpse = ResolveCorpseActor();
        // Post-pickup the server may destroy the corpse actor — from here
        // on only the inventory snapshot matters, not the corpse.
        if (bPickupRequested || IsValid(Corpse))
        {
            if (CountInventoryItems() > InitialInvCount || SumInventoryQuantities() > InitialInvQty)
            {
                BotAssert(true, TEXT("loot-inv-grew"), 1, 1);
                BotShot(TEXT("loot"));
                if (bQuestPhase)
                {
                    QuestLootDone(Now);
                    return true;
                }
                Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
                return true;
            }
        }
        if (!IsValid(Corpse))
        {
            // Corpse consumed by the pickup (server removes looted corpses):
            // keep polling the inventory until the pickup budget expires.
            // Corpse lost BEFORE any pickup request is a real failure.
            if (!bPickupRequested)
            {
                Finish(false, TEXT("corpse-gone"));
                return true;
            }
            if (Now - LootLegStartTime > LootTimeoutSec)
            {
                BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
                Finish(false, TEXT("pickup-timeout"));
            }
            return true;
        }
        if (CorpseUID <= 0)
        {
            CorpseUID = FCString::Atoi(*Corpse->GetMOBUId());
        }
        // Phase 1 (harvest_run only): harvest the corpse through the real
        // progress round-trip (server duration). loot_pickup skips straight
        // to inspect. Already-harvested corpses skip to inspect in both.
        if (bHarvestPhase && Corpse->CanBeHarvested())
        {
            if (!bHarvestRequested)
            {
                Player->SetLockedTarget(Corpse);
                Harvest->TryHarvestSpecificCorpse(Corpse);
                bHarvestRequested = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT harvest requested on corpse uid=%d"), CorpseUID);
            }
            if (Now - LootLegStartTime > HarvestTimeoutSec)
            {
                Finish(false, TEXT("harvest-timeout"));
            }
            return true;
        }
        // Phase 2: inspect the harvested corpse's loot.
        if (!bInspectRequested)
        {
            Harvest->InspectCorpseLoot(CorpseUID);
            bInspectRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT loot inspect requested uid=%d"), CorpseUID);
            return true;
        }
        // Server-confirmed empty corpse is a CONTENT signal (no loot table),
        // reported loudly rather than timing out — except in quest mode,
        // where an empty corpse just means this victim had no quest item:
        // move on through the same checkpoint (hunt next or turn in).
        if (Harvest->IsCorpseEmpty(CorpseUID))
        {
            if (bQuestPhase)
            {
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT corpse uid=%d empty, moving on (%d/%d)"),
                    CorpseUID, Kills, TargetKills);
                QuestLootDone(Now);
                return true;
            }
            BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
            Finish(false, TEXT("empty-corpse"));
            return true;
        }
        if (Harvest->GetCurrentAvailableLoot().Num() == 0)
        {
            if (Now - LootLegStartTime > LootTimeoutSec)
            {
                BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
                Finish(false, TEXT("inspect-timeout"));
            }
            return true;
        }
        // Phase 3: pick it all up, then fall through to the shared
        // inventory-growth poll below.
        if (!bPickupRequested)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT pickup-all requested (%d item(s))"),
                Harvest->GetCurrentAvailableLoot().Num());
            Harvest->PickupAllLoot();
            bPickupRequested = true;
            return true;
        }
        // Server confirmation lands as an inventory snapshot (stacks may
        // merge, so count OR quantity growth counts as proof).
        if (CountInventoryItems() > InitialInvCount || SumInventoryQuantities() > InitialInvQty)
        {
            BotAssert(true, TEXT("loot-inv-grew"), 1, 1);
            BotShot(TEXT("loot"));
            if (bQuestPhase)
            {
                QuestLootDone(Now);
                return true;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
            return true;
        }
        if (Now - LootLegStartTime > LootTimeoutSec)
        {
            BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
            Finish(false, TEXT("pickup-timeout"));
        }
        return true;
}

