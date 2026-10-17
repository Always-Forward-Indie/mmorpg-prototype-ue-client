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

bool UDevModeBotBrain::Tick_ExpectGround(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Grounded pre-flight: void falls must fail LOUD here, never slide
        // through the 2D asserts below. 5s grace for the drop+land to settle.
        const UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement();
        const bool bGrounded = MoveComp && MoveComp->IsMovingOnGround();
        if (bGrounded)
        {
            BotAssert(true, TEXT("spawn-grounded"), 1, 1);
            Step = EBotStep::GotoWaypoint;
            StepStartTime = Now;
            LastChasePos = PlayerAt;
            LastChaseProgressTime = Now;
            return true;
        }
        if (Now - StepStartTime > 5.0)
        {
            BotAssert(false, TEXT("spawn-grounded"), 0, 1);
            Finish(false, TEXT("no-ground"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_AssertKill(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        ABasicMOB* DeadMob = TargetMob.Get();
        const bool bDead = !IsValid(DeadMob) || DeadMob->GetMOBIsDead();
        BotAssert(bDead, TEXT("mob-dead"), bDead ? 1 : 0, 1);

        int32 Alive = 0;
        for (TActorIterator<ABasicMOB> It(World); It; ++It)
        {
            if (!(*It)->GetMOBIsDead()) { ++Alive; }
        }
        // Kills must drain the living count one by one (started at InitialAlive).
        BotAssert(Alive == InitialAlive - Kills - 1, TEXT("alive-count"), Alive, InitialAlive - Kills - 1);

        // Real locomotion is proven by the waypoint leg (odometer), not here.
        BotAssert(bAttackStarted, TEXT("attack-loop-active"), bAttackStarted ? 1 : 0, 1);

        if (!bDead)
        {
            Finish(false, TEXT("kill-not-registered"));
            return true;
        }
        if (IsValid(DeadMob))
        {
            LastKillLocation = DeadMob->GetActorLocation();
            CorpseUID = FCString::Atoi(*DeadMob->GetMOBUId());
        }
        ++Kills;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT kill %d/%d confirmed"), Kills, TargetKills);
        BotShot(FString::Printf(TEXT("kill%d"), Kills));

        if (Kills >= TargetKills)
        {
            // Loot leg walks back to the last corpse instead of quitting.
            if (bLootPhase)
            {
                Step = EBotStep::GotoCorpse;
                StepStartTime = Now;
                LootLegStartTime = Now;
                LastChasePos = PlayerAt;
                LastChaseProgressTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT kills done, walking to corpse (%.0f,%.0f)"),
                    LastKillLocation.X, LastKillLocation.Y);
                return true;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("all-kills-done") : TEXT("assert-failures"));
            return true;
        }
        bAttackStarted = false;
        TargetMob.Reset();
        Step = EBotStep::GotoTarget;
        StepStartTime = Now;
        LastChasePos = PlayerAt;
        LastChaseProgressTime = Now;
        return true;
}

bool UDevModeBotBrain::Tick_GotoWaypoint(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Deterministic locomotion proof before any combat: walk the leg.
        // Arrival radius 60 on a 150u leg guarantees >=90u of real walking;
        // threshold 50 keeps margin. Teleport jumps never count (odometer).
        if (SteerToward(Player, Waypoint, 60.0f, Now, TEXT("waypoint")))
        {
            BotAssert(Odometer > 50.0f, TEXT("player-walked"), (int32)Odometer, 50);
            if (bSkillsPhase)
            {
                Step = EBotStep::SkillWalk;
                CharLegStartTime = Now;
            }
            else if (bEquipPhase)
            {
                Step = EBotStep::EquipOn;
                CharLegStartTime = Now;
            }
            else if (bLevelPhase)
            {
                Step = EBotStep::LevelWait;
                CharLegStartTime = Now;
                StartLevel = GetProgressionLevel();
                StartExp = GetProgressionExp();
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT level baseline %d exp %d, waiting for grant"),
                    StartLevel, StartExp);
            }
            else if (bTitleEmotePhase)
            {
                Step = EBotStep::TitleLeg;
                CharLegStartTime = Now;
            }
            else if (bPeripheryPhase)
            {
                Step = EBotStep::LocaleLeg;
                FlowLegStartTime = Now;
            }
            else if (bCharFlowPhase)
            {
                Step = EBotStep::CharList;
                FlowLegStartTime = Now;
            }
            else if (bBackToLoginPhase)
            {
                Step = EBotStep::BackToLogin;
                FlowLegStartTime = Now;
            }
            else if (bQuestPhase)
            {
                // Quest chain: locomotion proven, walk to the quest NPC.
                Step = EBotStep::QuestWalk;
                QuestLegStartTime = Now;
            }
            else if (bVendorPhase)
            {
                // Economy run: locomotion proven, walk to the vendor next.
                Step = EBotStep::VendorWalk;
                ShopLegStartTime = Now;
            }
            else if (bDeathPhase)
            {
                // No combat in the death scenario: prove locomotion, then die.
                Step = EBotStep::ExpectDeath;
                DeathLegStartTime = Now;
            }
            else if (bRendezvousMode)
            {
                Step = EBotStep::GotoRendezvous;
            }
            else
            {
                Step = EBotStep::GotoTarget;
            }
            StepStartTime = Now;
            LastChasePos = Player->GetActorLocation();
            LastChaseProgressTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT waypoint reached (odometer %.0f)"), Odometer);
            BotShot(TEXT("waypoint"));
            return true;
        }
        if (Now - StepStartTime > GotoTimeoutSec)
        {
            Finish(false, TEXT("waypoint-timeout"));
        }
        return true;
}

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

bool UDevModeBotBrain::Tick_GotoTarget(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt, ABasicMOB* Mob, const FVector& MobAt, float Range, float Dist)
{
        // Stall sampling (all modes): a chase with no distance gain is
        // behind geometry the server won't let us cross — exclude the pick
        // and let the repick above sample the next one.
        const int32 ChaseUID = FCString::Atoi(*Mob->GetMOBUId());
        if (ChaseUID != ChaseTargetUID)
        {
            ChaseTargetUID = ChaseUID;
            ChaseCheckTime = Now;
            ChaseCheckDist = Dist;
        }
        else if (Now - ChaseCheckTime > ChaseStallSec)
        {
            if (ChaseCheckDist - Dist < ChaseMinGain)
            {
                ExcludedKillerUIDs.Add(ChaseUID);
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT chase of '%s' stalled (gain %.0f in %.0fs) — excluding uid=%d"),
                    *Mob->GetMobName(), ChaseCheckDist - Dist, ChaseStallSec, ChaseUID);
                TargetMob.Reset();
                ChaseTargetUID = 0;
                return true;
            }
            ChaseCheckTime = Now;
            ChaseCheckDist = Dist;
        }
        if (SteerToward(Player, MobAt, Range * ArrivalFraction, Now, *Mob->GetMobName()))
        {
            Player->SetLockedTarget(Mob);
            if (bPassiveCombat)
            {
                // Death leg (live): stand down in range, let the mob hit
                // back. No attack input — the kill must be server-side.
                Step = EBotStep::ExpectDeath;
                DeathLegStartTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT in range of '%s', waiting for real damage (passive)"),
                    *Mob->GetMobName());
                return true;
            }
            // In range: real lock + real attack loop from here on.
            Player->OnAttackInput();
            bAttackStarted = Player->bIsAutoAttacking;
            CombatStartTime = Now;
            StepStartTime = Now;
            Step = EBotStep::Combat;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT combat vs '%s' (dist %.0f, attack-loop=%d)"),
                *Mob->GetMobName(), Dist, bAttackStarted ? 1 : 0);
            return true;
        }

        if (Now - StepStartTime > GotoTimeoutSec)
        {
            Finish(false, TEXT("goto-timeout"));
        }
        return true;
}

bool UDevModeBotBrain::Tick_Combat(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt, ABasicMOB* Mob, const FVector& MobAt, float Range, float Dist)
{
        // Keep the real loop alive (cooldown/animation/approach code runs;
        // casts cannot confirm offline — no server — by design).
        if (Player->GetLockedTarget() != Mob)
        {
            Player->SetLockedTarget(Mob);
        }
        if (!Player->bIsAutoAttacking)
        {
            Player->OnAttackInput();
            bAttackStarted = bAttackStarted || Player->bIsAutoAttacking;
        }

        // Let the loop run briefly so animation/cooldown code executes, then
        // finish the mob: offline via the honest server-stub hit (same path as
        // devmode.setmobhp), live by waiting for the real server hit.
        if (bStubHits)
        {
            if (Now - CombatStartTime < 2.0f)
            {
                if (Now - StepStartTime > CombatTimeoutSec)
                {
                    Finish(false, TEXT("combat-timeout"));
                }
                return true;
            }

            UE_LOG(LogTemp, Warning, TEXT("%s_HIT_STUB '%s' (server-authoritative hit offline)"), *TombstonePrefix, *Mob->GetMobName());
            Mob->SetMOBCurrentHealth(0);
            Mob->Die();
        }
        else
        {
            if (!Mob->GetMOBIsDead() && Now - CombatStartTime > CombatKillTimeoutSec)
            {
                Finish(false, TEXT("no-server-damage"));
                return true;
            }
            if (!Mob->GetMOBIsDead())
            {
                // Offline only: the stub hit lands on a fixed beat, so a stall
                // here means broken plumbing. Live waits the full kill budget
                // above (real fights take tens of seconds).
                if (bStubHits && Now - StepStartTime > CombatTimeoutSec)
                {
                    Finish(false, TEXT("combat-timeout"));
                }
                return true;
            }
        }
        Step = EBotStep::AssertKill;
        StepStartTime = Now;
        return true;
}
