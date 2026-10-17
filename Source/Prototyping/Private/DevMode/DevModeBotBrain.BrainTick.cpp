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

void UDevModeBotBrain::BrainTick()
{
    if (!bRunning) { return; }
    UWorld* World = BrainWorld.Get();
    ABasicPlayer* Player = GameInstance ? GameInstance->Player : nullptr;
    if (!World || !Player || Player->IsActorBeingDestroyed())
    {
        Finish(false, TEXT("lost-world-or-player"));
        return;
    }

    const double Now = FPlatformTime::Seconds();

    // Death is fatal by default (death UI needs a human click) — but the
    // death_respawn scenario EXPECTS it: redirect into the assert leg
    // instead of failing. RespawnWait polls the revival itself.
    if (Player->GetIsDead())
    {
        if (!bDeathPhase)
        {
            Finish(false, TEXT("player-died"));
            return;
        }
        if (Step != EBotStep::AssertDead && Step != EBotStep::RespawnWait && Step != EBotStep::RecoverRevive)
        {
            Step = EBotStep::AssertDead;
            StepStartTime = Now;
        }
    }

    // (Re)acquire a living target whenever needed — except in AssertKill
    // (runs against the just-killed mob) and GotoWaypoint (needs no target).
    // Mid-combat death (server hit lands between ticks): do NOT repick —
    // fall through to AssertKill so the kill is counted, not skipped.
    if ((Step == EBotStep::GotoTarget || Step == EBotStep::Combat)
        && (!TargetMob.IsValid() || TargetMob->GetMOBIsDead()))
    {
        if (Step == EBotStep::Combat && TargetMob.IsValid())
        {
            Step = EBotStep::AssertKill;
        }
        else
        {
            // Exclusion set is death-mode-only in practice (empty for
            // combat runs) — shared helper keeps both honest.
            TargetMob = FindNearestAliveMobExcluding(ExcludedKillerUIDs);
            if (!TargetMob.IsValid())
            {
                Finish(Kills >= TargetKills, Kills >= TargetKills ? TEXT("no-targets-left") : TEXT("target-lost"));
                return;
            }
            Step = EBotStep::GotoTarget;
            StepStartTime = Now;
            LastChasePos = Player->GetActorLocation();
            LastChaseProgressTime = Now;
            UE_LOG(LogTemp, Log, TEXT("DEVMODE_BOT target '%s' at (%.0f,%.0f)"),
                *TargetMob->GetMobName(), TargetMob->GetActorLocation().X, TargetMob->GetActorLocation().Y);
        }
    }

    const FVector PlayerAt = Player->GetActorLocation();

    if (Step == EBotStep::ExpectGround)
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
            return;
        }
        if (Now - StepStartTime > 5.0)
        {
            BotAssert(false, TEXT("spawn-grounded"), 0, 1);
            Finish(false, TEXT("no-ground"));
        }
        return;
    }

    if (Step == EBotStep::AssertKill)
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
            return;
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
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("all-kills-done") : TEXT("assert-failures"));
            return;
        }
        bAttackStarted = false;
        TargetMob.Reset();
        Step = EBotStep::GotoTarget;
        StepStartTime = Now;
        LastChasePos = PlayerAt;
        LastChaseProgressTime = Now;
        return;
    }

    if (Step == EBotStep::GotoWaypoint)
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
            return;
        }
        if (Now - StepStartTime > GotoTimeoutSec)
        {
            Finish(false, TEXT("waypoint-timeout"));
        }
        return;
    }

    if (Step == EBotStep::GotoRendezvous)
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
            return;
        }
        if (Now - StepStartTime > RendezvousTimeoutSec)
        {
            Finish(false, TEXT("rendezvous-timeout"));
        }
        return;
    }

    if (Step == EBotStep::AssertVisible)
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
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("peer-visible") : TEXT("assert-failures"));
            return;
        }
        if (Now - StepStartTime > RendezvousTimeoutSec)
        {
            BotAssert(false, TEXT("peer-visible"), 0, 1);
            Finish(false, TEXT("peer-timeout"));
        }
        return;
    }

    if (Step == EBotStep::GotoCorpse)
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
            return;
        }
        if (Now - LootLegStartTime > LootTimeoutSec)
        {
            Finish(false, TEXT("corpse-timeout"));
        }
        return;
    }

    if (Step == EBotStep::LootPickup)
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
                    return;
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
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
            return;
        }
        // Live: the real game flow — harvest the victim's corpse, inspect
        // its loot, pick it all up. (World itemDrop packets on the dev
        // стенд carry empty item lists; corpse loot is the loot path.)
        UHarvestManager* Harvest = GameInstance->GetHarvestManager();
        if (!Harvest)
        {
            Finish(false, TEXT("no-harvest-manager"));
            return;
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
                    return;
                }
                Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
                return;
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
                return;
            }
            if (Now - LootLegStartTime > LootTimeoutSec)
            {
                BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
                Finish(false, TEXT("pickup-timeout"));
            }
            return;
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
            return;
        }
        // Phase 2: inspect the harvested corpse's loot.
        if (!bInspectRequested)
        {
            Harvest->InspectCorpseLoot(CorpseUID);
            bInspectRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT loot inspect requested uid=%d"), CorpseUID);
            return;
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
                return;
            }
            BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
            Finish(false, TEXT("empty-corpse"));
            return;
        }
        if (Harvest->GetCurrentAvailableLoot().Num() == 0)
        {
            if (Now - LootLegStartTime > LootTimeoutSec)
            {
                BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
                Finish(false, TEXT("inspect-timeout"));
            }
            return;
        }
        // Phase 3: pick it all up, then fall through to the shared
        // inventory-growth poll below.
        if (!bPickupRequested)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT pickup-all requested (%d item(s))"),
                Harvest->GetCurrentAvailableLoot().Num());
            Harvest->PickupAllLoot();
            bPickupRequested = true;
            return;
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
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("loot-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - LootLegStartTime > LootTimeoutSec)
        {
            BotAssert(false, TEXT("loot-inv-grew"), 0, 1);
            Finish(false, TEXT("pickup-timeout"));
        }
        return;
    }

    if (Step == EBotStep::ExpectDeath)
    {
        if (bStubDeath)
        {
            // Offline: no server damage exists, so the killing blow is an
            // honest stub through the real SetDead(true) path (same call the
            // server-driven death takes). Death screen + asserts are real.
            ApplyStubDeath(Player);
            DeathLegStartTime = Now;
            Step = EBotStep::AssertDead;
            StepStartTime = Now;
            return;
        }
        // Live: find a killer and stand down in range until real server
        // damage lands. Never attacks (bPassiveCombat handoff or direct).
        // Killers that deal zero damage for KillerDummySec are dummies
        // (wrong faction, leashed, geometry-separated) — exclude and move on.
        if (!bDeathWatchInit)
        {
            bDeathWatchInit = true;
            DeathWatchHP = Player->GetPlayerCurrentHPPoints();
            LastDamageTime = Now;
        }
        if (!TargetMob.IsValid() || TargetMob->GetMOBIsDead())
        {
            TargetMob = FindNearestAliveMobExcluding(ExcludedKillerUIDs);
            if (!TargetMob.IsValid())
            {
                Finish(false, TEXT("no-killer"));
                return;
            }
            // Fresh killer, fresh pull + fresh chase baseline.
            bProvoked = false;
            ChaseCheckTime = Now;
            ChaseCheckDist = Dist2D(PlayerAt, TargetMob->GetActorLocation());
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT offering target to '%s' (passive, dist %.0f)"),
                *TargetMob->GetMobName(), ChaseCheckDist);
        }
        ABasicMOB* Killer = TargetMob.Get();
        const float PassiveRange = Player->GetCurrentSkillRange();
        const float KillerDist = Dist2D(PlayerAt, Killer->GetActorLocation());
        if (KillerDist > PassiveRange * ArrivalFraction)
        {
            SteerToward(Player, Killer->GetActorLocation(), PassiveRange * ArrivalFraction, Now, *Killer->GetMobName());
            // No distance gain in ChaseStallSec: geometry the server won't
            // let us cross (moves get positionCorrection-reverted). Exclude
            // and sample the next killer instead of pushing a wall.
            if (Now - ChaseCheckTime > ChaseStallSec)
            {
                if (ChaseCheckDist - KillerDist < ChaseMinGain)
                {
                    const int32 StuckUID = FCString::Atoi(*Killer->GetMOBUId());
                    ExcludedKillerUIDs.Add(StuckUID);
                    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT killer '%s' unreachable (gain %.0f in %.0fs) — excluding uid=%d, switching"),
                        *Killer->GetMobName(), ChaseCheckDist - KillerDist, ChaseStallSec, StuckUID);
                    TargetMob.Reset();
                    return;
                }
                ChaseCheckTime = Now;
                ChaseCheckDist = KillerDist;
            }
            if (Now - DeathLegStartTime > DeathTimeoutSec)
            {
                Finish(false, TEXT("killer-unreachable"));
            }
            return;
        }
        if (Player->GetLockedTarget() != Killer)
        {
            Player->SetLockedTarget(Killer);
        }
        if (!bProvoked)
        {
            // Single honest pull: real server hit, starts the mob's
            // retaliation. The loop stops below — the bot must die, not win.
            Player->OnAttackInput();
            bProvoked = true;
            ProvokeTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT provoked '%s' (one hit, standing down next)"),
                *Killer->GetMobName());
        }
        else if (Player->bIsAutoAttacking && Now - ProvokeTime > 3.0)
        {
            Player->StopAutoAttack();
        }
        const int32 NowHP = Player->GetPlayerCurrentHPPoints();
        if (NowHP < DeathWatchHP)
        {
            // Taking real server damage — keep standing here.
            DeathWatchHP = NowHP;
            LastDamageTime = Now;
        }
        else if (Now - LastDamageTime > KillerDummySec)
        {
            const int32 DummyUID = FCString::Atoi(*Killer->GetMOBUId());
            ExcludedKillerUIDs.Add(DummyUID);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT killer '%s' dealt no damage in %.0fs — excluding uid=%d, switching"),
                *Killer->GetMobName(), KillerDummySec, DummyUID);
            TargetMob.Reset();
            DeathWatchHP = NowHP;
            LastDamageTime = Now;
            return;
        }
        if (Now - DeathLegStartTime > DeathTimeoutSec)
        {
            Finish(false, FString::Printf(TEXT("no-death hp=%d dist=%.0f"), NowHP, KillerDist));
        }
        // Still alive: keep standing here; the top-of-tick dead check
        // redirects to AssertDead the moment a server hit lands.
        return;
    }

    if (Step == EBotStep::AssertDead)
    {
        // Entered only while dead (top-of-tick redirect or stub handoff).
        if (!Player->GetIsDead())
        {
            Finish(false, TEXT("death-not-registered"));
            return;
        }
        if (!bDeathAsserted)
        {
            bDeathAsserted = true;
            BotAssert(true, TEXT("player-dead"), 1, 1);
            const bool bScreen = IsDeathScreenVisible();
            BotAssert(bScreen, TEXT("death-screen"), bScreen ? 1 : 0, 1);
            BotShot(TEXT("death"));
        }
        // The screenshot pipe holds one request: revive only after the death
        // shot flushes, or the revive/respawned burst overwrites it and the
        // checkpoint is lost (single-slot pipe, flush takes ~0.1-0.3s).
        if (FScreenshotRequest::IsScreenshotRequested() || !PendingShotName.IsEmpty())
        {
            UE_LOG(LogTemp, Log, TEXT("DEVMODE_BOT death shot flushing, revive next tick"));
            return;
        }
        if (bStubDeath)
        {
            ApplyStubRevive(Player);
        }
        else if (!bRespawnRequested)
        {
            Player->OnRespawnClicked();
            bRespawnRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT respawn requested"));
        }
        RespawnLegStartTime = FPlatformTime::Seconds();
        Step = EBotStep::RespawnWait;
        StepStartTime = RespawnLegStartTime;
        return;
    }

    if (Step == EBotStep::RespawnWait)
    {
        // Offline revive is synchronous (stub); live waits for the server
        // respawnResult packet that clears the dead flag.
        if (Player->GetIsDead())
        {
            if (!bStubDeath && !bRespawnRequested)
            {
                Player->OnRespawnClicked();
                bRespawnRequested = true;
            }
            if (FPlatformTime::Seconds() - RespawnLegStartTime > RespawnTimeoutSec)
            {
                BotAssert(false, TEXT("player-revived"), 0, 1);
                Finish(false, TEXT("respawn-timeout"));
            }
            return;
        }
        const int32 HP = Player->GetPlayerCurrentHPPoints();
        if (HP <= 0)
        {
            // Live: the dead flag clears in respawnResult, the HP snapshot
            // (stats_update) arrives a beat later. Wait for it briefly
            // instead of asserting on the stale zero.
            const double NowHP = FPlatformTime::Seconds();
            if (ReviveSeenTime <= 0.0)
            {
                ReviveSeenTime = NowHP;
                BotAssert(true, TEXT("player-revived"), 1, 1);
            }
            if (NowHP - ReviveSeenTime > 10.0)
            {
                BotAssert(false, TEXT("respawn-hp"), HP, 1);
                Finish(false, TEXT("respawn-hp-timeout"));
            }
            return;
        }
        if (ReviveSeenTime <= 0.0)
        {
            ReviveSeenTime = FPlatformTime::Seconds();
            BotAssert(true, TEXT("player-revived"), 1, 1);
        }
        BotAssert(HP > 0, TEXT("respawn-hp"), HP, 1);
        const bool bScreenHidden = !IsDeathScreenVisible();
        BotAssert(bScreenHidden, TEXT("death-screen-hidden"), bScreenHidden ? 1 : 0, 1);
        if (bStubDeath)
        {
            // Offline revive point is deterministic (run spawn).
            const float Home = Dist2D(Player->GetActorLocation(), RespawnPoint);
            BotAssert(Home < 300.0f, TEXT("respawn-point"), (int32)Home, 0);
        }
        BotShot(TEXT("respawned"));
        Finish(Failures == 0, Failures == 0 ? TEXT("death-respawn-done") : TEXT("assert-failures"));
        return;
    }

    if (Step == EBotStep::RecoverRevive)
    {
        // Stale server-side death (previous run never revived): wait for
        // the revival, then start the run over. Never asserts — recovery
        // is hygiene, the scenario proof comes afterwards.
        if (!Player->GetIsDead())
        {
            RunStartPos = Player->GetActorLocation();
            // Re-aim the locomotion waypoint from the revive point (the one
            // computed at Start belongs to the death position).
            FVector ToMob = FVector(300.0f, 0.0f, 0.0f);
            if (ABasicMOB* FirstMob = FindNearestAliveMob())
            {
                ToMob = FirstMob->GetActorLocation() - RunStartPos;
                ToMob.Z = 0.0f;
                if (ToMob.Size() < 1.0f) { ToMob = FVector(300.0f, 0.0f, 0.0f); }
                ToMob.Normalize();
                ToMob *= 150.0f;
            }
            Waypoint = RunStartPos + ToMob;
            Waypoint.Z = RunStartPos.Z;
            OdometerAnchor = RunStartPos;
            LastChasePos = RunStartPos;
            LastChaseProgressTime = Now;
            Step = EBotStep::GotoWaypoint;
            StepStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT recovered from stale death, starting run"));
            return;
        }
        if (FPlatformTime::Seconds() - RespawnLegStartTime > RespawnTimeoutSec)
        {
            Finish(false, TEXT("spawned-dead"));
        }
        return;
    }

    if (Step == EBotStep::VendorWalk)
    {
        // Honest walk to the vendor (server validates range from its
        // stored position — teleports would fail the shop open).
        if (SteerToward(Player, VendorPos, 150.0f, Now, TEXT("vendor")))
        {
            Step = EBotStep::VendorSell;
            ShopLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at vendor npc=%d, opening shop"), VendorNpcId);
            BotShot(TEXT("vendor"));
            return;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            Finish(false, TEXT("vendor-unreachable"));
        }
        return;
    }

    if (Step == EBotStep::VendorSell)
    {
        UVendorManager* Vend = GameInstance->GetVendorManager();
        UInventoryManager* Inv = GameInstance->GetInventoryManager();
        if (!Vend || !Inv)
        {
            Finish(false, TEXT("no-vendor"));
            return;
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
                    return;
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
            return;
        }
        if (!bSellRequested)
        {
            Vend->RequestSellItem(CharId, VendorNpcId, SellInstanceId, 1, Player->GetActorLocation());
            bSellRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT sell requested instance=%d"), SellInstanceId);
            return;
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
            return;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            BotAssert(false, TEXT("sell-gold-up"), GetGold(), GoldBeforeSell + 1);
            Finish(false, TEXT("sell-timeout"));
        }
        return;
    }

    if (Step == EBotStep::VendorRepair)
    {
        URepairManager* Rep = GameInstance->GetRepairManager();
        if (!Rep)
        {
            Finish(false, TEXT("no-repair"));
            return;
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
                    return;
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
            return;
        }
        if (!bRepairRequested)
        {
            Rep->RequestRepairItem(CharId, VendorNpcId, RepairInstanceId, Player->GetActorLocation());
            bRepairRequested = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT repair requested instance=%d"), RepairInstanceId);
            return;
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
            return;
        }
        if (Now - ShopLegStartTime > ShopTimeoutSec)
        {
            BotAssert(false, TEXT("repair-durability-max"), NowDur, RepairMaxDur);
            Finish(false, TEXT("repair-timeout"));
        }
        return;
    }

    if (Step == EBotStep::ChatExchange)
    {
        UChatManager* Chat = GameInstance->GetChatManager();
        if (!Chat)
        {
            Finish(false, TEXT("no-chat"));
            return;
        }
        const int32 OwnCharId = GameInstance->GetCurrentCharacterID();
        if (!bChatSent)
        {
            // Local chat only travels 50u server-side. Both sides anchor to
            // the SAME static rendezvous point (arrive tight) instead of
            // chasing the peer actor — replicated peer positions lag too
            // much to converge on. Then settle (server positions lag the
            // client tick) and speak.
            if (Dist2D(PlayerAt, RendezvousPoint) > 20.0f)
            {
                PeerCloseSince = 0.0;
                SteerToward(Player, RendezvousPoint, 20.0f, Now, TEXT("chat-anchor"));
                if (Now - ChatLegStartTime > ChatTimeoutSec)
                {
                    Finish(false, TEXT("peer-unreachable"));
                }
                return;
            }
            if (!IsValid(FindPeerActor()))
            {
                Finish(false, TEXT("peer-lost"));
                return;
            }
            if (PeerCloseSince <= 0.0)
            {
                PeerCloseSince = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT anchored at rendezvous, settling before speaking"));
                return;
            }
            if (Now - PeerCloseSince < 3.0)
            {
                return;
            }
            // One short "local" message each; both sides send, both assert
            // the peer's arrival (own echo, if any, is filtered by sender).
            Chat->SendChatMessage(TEXT("local"), FString::Printf(TEXT("hi-%d"), OwnCharId));
            bChatSent = true;
            ChatLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT chat sent hi-%d from (%.0f,%.0f,%.0f)"),
                OwnCharId, PlayerAt.X, PlayerAt.Y, PlayerAt.Z);
            return;
        }
        // Peer message = hi-* from anyone but us.
        PeerChatText.Empty();
        for (const FChatMessageStruct& Msg : Chat->GetMessageLog())
        {
            if (!Msg.bIsError && Msg.senderId > 0 && Msg.senderId != OwnCharId && Msg.text.StartsWith(TEXT("hi-")))
            {
                PeerChatText = Msg.text;
                break;
            }
        }
        if (!PeerChatText.IsEmpty())
        {
            BotAssert(true, TEXT("peer-msg-received"), 1, 1);
            // The bubble renders on the peer actor's nameplate through the
            // same ShowChatBubble call live players trigger.
            bool bBubbleOk = false;
            if (ABasicPlayer* Peer = FindPeerActor())
            {
                if (UPlayerNameplateComponent* Plate = Peer->GetNameplateComponent())
                {
                    bBubbleOk = Plate->GetLastBubbleText() == PeerChatText;
                }
            }
            BotAssert(bBubbleOk, TEXT("peer-bubble"), bBubbleOk ? 1 : 0, 1);
            BotShot(TEXT("chat"));
            Finish(Failures == 0, Failures == 0 ? TEXT("chat-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - ChatLegStartTime > ChatTimeoutSec)
        {
            BotAssert(false, TEXT("peer-msg-received"), 0, 1);
            Finish(false, TEXT("chat-timeout"));
        }
        return;
    }

    if (Step == EBotStep::QuestWalk)
    {
        // Honest walk to the quest giver (dialogue range is server-side).
        if (SteerToward(Player, QuestNpcPos, 150.0f, Now, TEXT("quest-npc")))
        {
            Step = EBotStep::QuestAccept;
            QuestLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at quest npc=%d, opening dialogue"), QuestNpcId);
            return;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            Finish(false, TEXT("quest-unreachable"));
        }
        return;
    }

    if (Step == EBotStep::QuestAccept)
    {
        UDialogueManager* Dlg = GameInstance->GetDialogueManager();
        UQuestManager* Qst = GameInstance->GetQuestManager();
        if (!Dlg || !Qst)
        {
            Finish(false, TEXT("no-dialogue"));
            return;
        }
        // Accepted already (choice landed, or a rerun mid-chain): hunt.
        if (Qst->HasQuest(QuestId))
        {
            BotAssert(true, TEXT("quest-accepted"), QuestId, QuestId);
            BotShot(TEXT("accepted"));
            if (Dlg->IsDialogueActive())
            {
                Dlg->CloseDialogue();
            }
            // Objectives run on the shared combat/loot machinery (2 real
            // kills + harvest + loot); the server credits them to the quest.
            TargetMob.Reset();
            Step = EBotStep::GotoTarget;
            StepStartTime = Now;
            LastChasePos = PlayerAt;
            LastChaseProgressTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT quest accepted, hunting objectives"));
            return;
        }
        if (!EnsureDialogueOpen(QuestNpcId, Now))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("accept-timeout"));
            }
            return;
        }
        if (const int32 AcceptEdge = FindGoalEdge(false))
        {
            if (!bChoiceSent)
            {
                Dlg->SendChoice(AcceptEdge);
                bChoiceSent = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT accept choice sent edge=%d"), AcceptEdge);
            }
            // Quest grant lands as a journal push; polled at the top on the
            // next ticks (HasQuest branch above).
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("accept-timeout"));
            }
            return;
        }
        // No offer on this node: walk the dialogue (greeting -> hub).
        if (DialogueClicks >= 10)
        {
            Finish(false, TEXT("dialogue-loop"));
            return;
        }
        const int32 NodeId = Dlg->GetCurrentNode().nodeId;
        if (const int32 AdvanceEdge = FindAdvanceEdge())
        {
            if (NodeId != LastAdvanceNodeId)
            {
                Dlg->SendChoice(AdvanceEdge);
                LastAdvanceNodeId = NodeId;
                ++DialogueClicks;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT dialogue advance edge=%d (node %d)"),
                    AdvanceEdge, NodeId);
            }
            return;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT no accept/advance edge in node %d (%d choices)"),
                NodeId, Dlg->GetCurrentNode().choices.Num());
            Finish(false, TEXT("no-accept-choice"));
        }
        return;
    }

    if (Step == EBotStep::QuestTurnin)
    {
        UDialogueManager* Dlg = GameInstance->GetDialogueManager();
        UQuestManager* Qst = GameInstance->GetQuestManager();
        if (!Dlg || !Qst)
        {
            Finish(false, TEXT("no-dialogue"));
            return;
        }
        // Walk back first (objectives may have pulled us far away). The
        // server gates dialogue on proximity: only talk once arrived.
        if (!SteerToward(Player, QuestNpcPos, 150.0f, Now, TEXT("turnin-npc")))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("turnin-unreachable"));
            }
            return;
        }
        if (!EnsureDialogueOpen(QuestNpcId, Now))
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                Finish(false, TEXT("turnin-timeout"));
            }
            return;
        }
        if (const int32 TurninEdge = FindGoalEdge(true))
        {
            if (!bChoiceSent)
            {
                Dlg->SendChoice(TurninEdge);
                bChoiceSent = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turnin choice sent edge=%d"), TurninEdge);
                return;
            }
        }
        // Reward ground truth (checked regardless of the current node: the
        // turn-in choice advances past the turn-in edge): +5g floor (loot
        // gold may add more) and the freshly granted potion.
        if (bChoiceSent && GetGold() >= GoldBeforeReward + 5 && HasItemTemplate(46))
        {
            const FQuestProgressData Q = Qst->GetQuestData(QuestId);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turned in: gold %d -> %d queststate='%s'"),
                GoldBeforeReward, GetGold(), *Q.state);
            BotAssert(true, TEXT("quest-reward-gold"), 1, 1);
            BotAssert(true, TEXT("quest-reward-item"), 46, 46);
            BotShot(TEXT("turnedin"));
            Dlg->CloseDialogue();
            Finish(Failures == 0, Failures == 0 ? TEXT("quest-done") : TEXT("assert-failures"));
            return;
        }
        if (bChoiceSent)
        {
            if (Now - QuestLegStartTime > QuestTimeoutSec)
            {
                BotAssert(false, TEXT("quest-reward-gold"), GetGold(), GoldBeforeReward + 5);
                Finish(false, TEXT("turnin-timeout"));
            }
            return;
        }
        // Objectives not yet credited server-side, or a hub without the
        // turn-in: advance neutral hubs, budget the silence.
        if (DialogueClicks >= 10)
        {
            Finish(false, TEXT("dialogue-loop"));
            return;
        }
        const int32 NodeId = Dlg->GetCurrentNode().nodeId;
        if (const int32 AdvanceEdge = FindAdvanceEdge())
        {
            if (NodeId != LastAdvanceNodeId)
            {
                Dlg->SendChoice(AdvanceEdge);
                LastAdvanceNodeId = NodeId;
                ++DialogueClicks;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT turnin dialogue advance edge=%d (node %d)"),
                    AdvanceEdge, NodeId);
            }
            return;
        }
        if (Now - QuestLegStartTime > QuestTimeoutSec)
        {
            const FQuestProgressData Q = Qst->GetQuestData(QuestId);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT no turnin edge in node %d (%d choices) queststate='%s' step='%s' %d/%d"),
                NodeId, Dlg->GetCurrentNode().choices.Num(), *Q.state, *Q.currentStepKey, Q.stepCurrentCount, Q.stepRequiredCount);
            Finish(false, TEXT("no-turnin-choice"));
        }
        return;
    }

    if (Step == EBotStep::SkillWalk)
    {
        // Honest walk to the trainer (shop range is server-side).
        if (SteerToward(Player, SkillNpcPos, 150.0f, Now, TEXT("trainer")))
        {
            Step = EBotStep::SkillLearn;
            CharLegStartTime = Now;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT at trainer npc=%d, opening skill shop"), SkillNpcId);
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            Finish(false, TEXT("skills-unreachable"));
        }
        return;
    }

    if (Step == EBotStep::SkillLearn)
    {
        USkillShopManager* Shop = GameInstance->GetSkillShopManager();
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!Shop || !PSM)
        {
            Finish(false, TEXT("no-skills"));
            return;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        if (Shop->GetCurrentShop().npcId != SkillNpcId)
        {
            if (!bShopReqSent)
            {
                Shop->RequestOpenSkillShop(SkillNpcId, Player->GetActorLocation());
                bShopReqSent = true;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT skill shop requested npc=%d"), SkillNpcId);
            }
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                Finish(false, TEXT("skill-shop-timeout"));
            }
            return;
        }
        BotAssert(true, TEXT("skill-shop-open"), SkillNpcId, SkillNpcId);
        // Learn power_slash then battle_cry (one in flight at a time).
        FString Next;
        if (!PSM->HasSkill(TEXT("power_slash"))) { Next = TEXT("power_slash"); }
        else if (!PSM->HasSkill(TEXT("battle_cry"))) { Next = TEXT("battle_cry"); }
        if (!Next.IsEmpty())
        {
            if (!bLearnRequested || LastLearnSlug != Next)
            {
                Shop->RequestLearnSkill(SkillNpcId, Next);
                bLearnRequested = true;
                LastLearnSlug = Next;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT learn requested %s"), *Next);
            }
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                BotAssert(false, TEXT("skill-learned"), 0, 1);
                Finish(false, TEXT("learn-timeout"));
            }
            return;
        }
        BotAssert(true, TEXT("skill-learned-power"), 1, 1);
        BotAssert(true, TEXT("skill-learned-cry"), 1, 1);
        BotShot(TEXT("learned"));
        Step = EBotStep::SkillCast;
        CharLegStartTime = Now;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT skills learned, hunting cast target"));
        return;
    }

    if (Step == EBotStep::SkillCast)
    {
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!PSM)
        {
            Finish(false, TEXT("no-skills"));
            return;
        }
        if (!TargetMob.IsValid() || TargetMob->GetMOBIsDead())
        {
            if (bCastSent && TargetMob.IsValid())
            {
                // Our hit landed lethally: the deferred server death flips
                // the flag instead of an HP number. Damage proven — keep the
                // corpse pointer (cooldown/mana latches still poll below),
                // do NOT repick and lose the kill.
                bDamageSeen = true;
            }
            else
            {
                TargetMob = FindNearestAliveMobExcluding(ExcludedKillerUIDs);
                if (!TargetMob.IsValid())
                {
                    Finish(false, TEXT("no-cast-target"));
                    return;
                }
                ChaseTargetUID = 0;
            }
        }
        ABasicMOB* Target = TargetMob.Get();
        const int32 TargetUid = FCString::Atoi(*Target->GetMOBUId());
        const float CastRange = Player->GetCurrentSkillRange();
        // Post-cast the mob may flee: poll the hit regardless of range.
        // The range gate applies only to the first cast. Server
        // confirmation lands piecemeal: cooldown arms on initiation, mana
        // on stats_update, damage on the deferred result flush (which may
        // lag ~12s and may only flip the dead flag — accept HP drop OR
        // death as the damage proof).
        if (bCastSent)
        {
            const int32 MobHpNow = Target->GetMOBCurrentHealth();
            const int32 ManaNow = Player->GetPlayerCurrentMPPoints();
            bDamageSeen = bDamageSeen || (MobHpNow < MobHpBefore) || Target->GetMOBIsDead();
            bCooldownSeen = bCooldownSeen || PSM->IsSkillOnCooldown(TEXT("power_slash"));
            bManaSeen = bManaSeen || (ManaNow < ManaBefore);
            if (bDamageSeen && bCooldownSeen && bManaSeen)
            {
                BotAssert(true, TEXT("skill-damage"), 1, 1);
                BotAssert(true, TEXT("skill-cooldown"), 1, 1);
                BotAssert(true, TEXT("skill-mana"), 1, 1);
                BotShot(TEXT("cast"));
                Step = EBotStep::SkillBuff;
                CharLegStartTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT power_slash confirmed: mobhp %d -> %d mana %d -> %d"),
                    MobHpBefore, MobHpNow, ManaBefore, ManaNow);
                return;
            }
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                BotAssert(bDamageSeen, TEXT("skill-damage"), bDamageSeen ? 1 : 0, 1);
                BotAssert(bCooldownSeen, TEXT("skill-cooldown"), bCooldownSeen ? 1 : 0, 1);
                BotAssert(bManaSeen, TEXT("skill-mana"), bManaSeen ? 1 : 0, 1);
                Finish(false, TEXT("cast-timeout"));
            }
            return;
        }
        if (Dist2D(PlayerAt, Target->GetActorLocation()) > CastRange * ArrivalFraction)
        {
            SteerToward(Player, Target->GetActorLocation(), CastRange * ArrivalFraction, Now, *Target->GetMobName());
            if (Now - CharLegStartTime > CharTimeoutSec)
            {
                Finish(false, TEXT("cast-unreachable"));
            }
            return;
        }
        if (Player->GetLockedTarget() != Target)
        {
            Player->SetLockedTarget(Target);
        }
        MobHpBefore = Target->GetMOBCurrentHealth();
        ManaBefore = Player->GetPlayerCurrentMPPoints();
        PSM->TryCastSkill(TEXT("power_slash"), TargetUid, ECasterType::Mob);
        bCastSent = true;
        bDamageSeen = false;
        bCooldownSeen = false;
        bManaSeen = false;
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT power_slash cast on uid=%d (mobhp=%d mana=%d)"),
            TargetUid, MobHpBefore, ManaBefore);
        return;
    }

    if (Step == EBotStep::SkillBuff)
    {
        UPlayerSkillManager* PSM = GameInstance->GetPlayerSkillManager();
        if (!PSM)
        {
            Finish(false, TEXT("no-skills"));
            return;
        }
        if (!bBuffSent)
        {
            PSM->TryCastSkill(TEXT("battle_cry"), 0, ECasterType::Player);
            bBuffSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT battle_cry cast (self)"));
            return;
        }
        // The buff lands as a server stats_update activeEffects entry —
        // the same snapshot ActiveEffectsWidget renders.
        if (HasActiveEffect(TEXT("battle_cry_phys_atk")))
        {
            BotAssert(true, TEXT("buff-active"), 1, 1);
            BotShot(TEXT("buff"));
            Finish(Failures == 0, Failures == 0 ? TEXT("skills-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("buff-active"), 0, 1);
            Finish(false, TEXT("buff-timeout"));
        }
        return;
    }

    if (Step == EBotStep::EquipOn)
    {
        UEquipmentManager* EqMgr = GameInstance->GetEquipmentManager();
        if (!EqMgr)
        {
            Finish(false, TEXT("no-equipment"));
            return;
        }
        const int32 CharId = GameInstance->GetCurrentCharacterID();
        const FEquipmentSlotData Slot = EqMgr->GetSlot(EquipSlotSlug);
        if (Slot.bIsOccupied && EquipInstanceId <= 0)
        {
            // Idempotent entry: already equipped (e.g. a previous run left
            // it on) — verify slot + visual, then continue the round-trip.
            bWasPreEquipped = true;
            EquipInstanceId = Slot.inventoryItemId;
            EquipItemSlug = Slot.itemSlug;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT slot %s pre-equipped instance=%d, verifying"),
                *EquipSlotSlug, EquipInstanceId);
        }
        if (EquipInstanceId <= 0)
        {
            EquipInstanceId = FindEquippableMainHand(EquipItemSlug);
            if (EquipInstanceId <= 0)
            {
                Finish(false, TEXT("no-equippable"));
                return;
            }
            EqMgr->RequestEquipItem(CharId, EquipInstanceId);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT equip requested instance=%d '%s'"),
                EquipInstanceId, *EquipItemSlug);
            return;
        }
        if (Slot.bIsOccupied && Slot.inventoryItemId == EquipInstanceId)
        {
            BotAssert(true, TEXT("equip-slot"), 1, 1);
            // Visual proof: the mesh the game attached for this slot.
            bool bVisualOk = false;
            if (UEquipmentVisualComponent* Vis = Player->GetEquipmentVisualComponent())
            {
                bVisualOk = Vis->GetItemSlugForSlot(EquipSlotSlug) == EquipItemSlug
                    && Vis->GetSlotComponent(EquipSlotSlug) != nullptr;
            }
            BotAssert(bVisualOk, TEXT("equip-visual"), bVisualOk ? 1 : 0, 1);
            BotShot(TEXT("equipped"));
            if (bRestorePass)
            {
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT pre-equipped item restored"));
                Finish(Failures == 0, Failures == 0 ? TEXT("equip-done") : TEXT("assert-failures"));
                return;
            }
            Step = EBotStep::EquipOff;
            CharLegStartTime = Now;
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("equip-slot"), 0, EquipInstanceId);
            Finish(false, TEXT("equip-timeout"));
        }
        return;
    }

    if (Step == EBotStep::EquipOff)
    {
        UEquipmentManager* EqMgr = GameInstance->GetEquipmentManager();
        if (!EqMgr)
        {
            Finish(false, TEXT("no-equipment"));
            return;
        }
        if (!bUnequipSent)
        {
            EqMgr->RequestUnequipItem(GameInstance->GetCurrentCharacterID(), EquipSlotSlug);
            bUnequipSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT unequip requested slot=%s"), *EquipSlotSlug);
            return;
        }
        const FEquipmentSlotData Slot = EqMgr->GetSlot(EquipSlotSlug);
        if (!Slot.bIsOccupied)
        {
            BotAssert(true, TEXT("unequip-slot"), 1, 1);
            BotShot(TEXT("unequipped"));
            if (bWasPreEquipped)
            {
                // Restore the pre-run state, verify once, then finish.
                EqMgr->RequestEquipItem(GameInstance->GetCurrentCharacterID(), EquipInstanceId);
                bWasPreEquipped = false;
                bUnequipSent = false;
                bRestorePass = true;
                EquipInstanceId = 0;
                Step = EBotStep::EquipOn;
                CharLegStartTime = Now;
                UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT restoring pre-equipped item"));
                return;
            }
            Finish(Failures == 0, Failures == 0 ? TEXT("equip-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("unequip-slot"), 1, 0);
            Finish(false, TEXT("unequip-timeout"));
        }
        return;
    }

    if (Step == EBotStep::LevelWait)
    {
        // XP is granted externally mid-run (admin grant_xp): the bot only
        // polls the authoritative progression snapshot.
        const int32 LevelNow = GetProgressionLevel();
        const int32 ExpNow = GetProgressionExp();
        if (LevelNow > StartLevel && ExpNow > StartExp)
        {
            BotAssert(true, TEXT("level-grew"), LevelNow, StartLevel + 1);
            BotAssert(true, TEXT("exp-grew"), 1, 1);
            BotShot(TEXT("levelup"));
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT leveled: %d -> %d exp %d -> %d"),
                StartLevel, LevelNow, StartExp, ExpNow);
            Finish(Failures == 0, Failures == 0 ? TEXT("level-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("level-grew"), LevelNow, StartLevel + 1);
            Finish(false, TEXT("level-timeout"));
        }
        return;
    }

    if (Step == EBotStep::TitleLeg)
    {
        UTitleNetworkHandler* TitleNet = GameInstance->GetTitleNetworkHandler();
        UTitleManager* Titles = GameInstance->GetTitleManager();
        if (!TitleNet || !Titles)
        {
            Finish(false, TEXT("no-titles"));
            return;
        }
        if (!bTitleSent)
        {
            TitleNet->RequestEquipTitle(GameInstance->GetCurrentCharacterID(), TEXT("first_hunter"));
            bTitleSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT equip title first_hunter"));
            return;
        }
        if (Titles->GetEquippedTitleSlug() == TEXT("first_hunter"))
        {
            BotAssert(true, TEXT("title-equipped"), 1, 1);
            BotShot(TEXT("title"));
            Step = EBotStep::EmoteLeg;
            CharLegStartTime = Now;
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("title-equipped"), 0, 1);
            Finish(false, TEXT("title-timeout"));
        }
        return;
    }

    if (Step == EBotStep::EmoteLeg)
    {
        UEmoteNetworkHandler* EmoteNet = GameInstance->GetEmoteNetworkHandler();
        UEmoteManager* Emotes = GameInstance->GetEmoteManager();
        if (!EmoteNet || !Emotes)
        {
            Finish(false, TEXT("no-emotes"));
            return;
        }
        if (!bEmoteSent)
        {
            // Play the first owned emote from the server snapshot — never
            // assume a slug is unlocked (salute is NOT default; wave is).
            const TArray<FEmoteDefinitionData>& Owned = Emotes->GetPlayerEmotesState().emotes;
            if (Owned.Num() == 0)
            {
                Finish(false, TEXT("no-emotes"));
                return;
            }
            EmoteSlug = Owned[0].slug;
            EmoteNet->RequestUseEmote(EmoteSlug);
            bEmoteSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT emote %s sent"), *EmoteSlug);
            return;
        }
        // The server broadcasts back (sender in own range): our actor plays
        // through the same PlayEmoteForCharacter path as any player.
        bool bPlaying = false;
        if (UEmoteComponent* Emote = Player->GetEmoteComponent())
        {
            bPlaying = Emote->IsPlayingEmote() && Emote->GetCurrentEmoteSlug() == EmoteSlug;
        }
        if (bPlaying)
        {
            BotAssert(true, TEXT("emote-playing"), 1, 1);
            BotShot(TEXT("emote"));
            Finish(Failures == 0, Failures == 0 ? TEXT("titles-emotes-done") : TEXT("assert-failures"));
            return;
        }
        if (Now - CharLegStartTime > CharTimeoutSec)
        {
            BotAssert(false, TEXT("emote-playing"), 0, 1);
            Finish(false, TEXT("emote-timeout"));
        }
        return;
    }

    if (Step == EBotStep::LocaleLeg)
    {
        ULocalizationSubsystem* Loc = GameInstance->GetSubsystem<ULocalizationSubsystem>();
        if (!Loc)
        {
            Finish(false, TEXT("no-locale"));
            return;
        }
        SavedLocale = Loc->GetCurrentLocale();
        Loc->SetLocale(TEXT("ru"));
        const FText NameRu = Loc->GetItemDisplayName(TEXT("health_potion"));
        Loc->SetLocale(TEXT("en"));
        const FText NameEn = Loc->GetItemDisplayName(TEXT("health_potion"));
        BotAssert(!NameRu.IsEmpty(), TEXT("locale-ru"), 1, 1);
        BotAssert(!NameEn.IsEmpty(), TEXT("locale-en"), 1, 1);
        BotAssert(Loc->GetCurrentLocale() == TEXT("en"), TEXT("locale-switch"), 1, 1);
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT locale ru='%s' en='%s'"),
            *NameRu.ToString(), *NameEn.ToString());
        Loc->SetLocale(SavedLocale);
        BotAssert(Loc->GetCurrentLocale() == SavedLocale, TEXT("locale-restore"), 1, 1);
        BotShot(TEXT("locale"));
        // Camera before music: music needs designer BP content that may be
        // absent (loud FAIL below) — camera must still get verified.
        Step = EBotStep::CameraLeg;
        FlowLegStartTime = FPlatformTime::Seconds();
        return;
    }

    if (Step == EBotStep::MusicLeg)
    {
        UAudioManager* Audio = GameInstance->GetAudioManager();
        if (!Audio)
        {
            Finish(false, TEXT("no-audio"));
            return;
        }
        if (Audio->ZoneMusicMap.Num() < 2)
        {
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT music zones mapped: %d (need 2)"), Audio->ZoneMusicMap.Num());
            Finish(false, TEXT("no-music-zones"));
            return;
        }
        const FString ZoneA = Audio->ZoneMusicMap[0].ZoneName;
        const FString ListA = Audio->ZoneMusicMap[0].PlaylistId;
        const FString ZoneB = Audio->ZoneMusicMap[1].ZoneName;
        const FString ListB = Audio->ZoneMusicMap[1].PlaylistId;
        if (MusicStage == 0)
        {
            Audio->PlayMusicForZone(ZoneA);
            MusicStage = 1;
            return;
        }
        if (MusicStage == 1 && Audio->GetActivePlaylistId() == ListA)
        {
            BotAssert(true, TEXT("music-zone-a"), 1, 1);
            Audio->PlayMusicForZone(ZoneB);
            MusicStage = 2;
            return;
        }
        if (MusicStage == 2 && Audio->GetActivePlaylistId() == ListB)
        {
            BotAssert(true, TEXT("music-zone-b"), 1, 1);
            BotShot(TEXT("music"));
            Finish(Failures == 0, Failures == 0 ? TEXT("periphery-done") : TEXT("assert-failures"));
            return;
        }
        if (FPlatformTime::Seconds() - FlowLegStartTime > FlowTimeoutSec)
        {
            BotAssert(false, TEXT("music-switch"), 0, 1);
            Finish(false, TEXT("music-timeout"));
        }
        return;
    }

    if (Step == EBotStep::CameraLeg)
    {
        // Zoom through the real input path (OnScroll), assert clamp ends
        // and boom convergence. No server involved.
        const float ZoomMin = Player->GetZoomMin();
        const float ZoomMax = Player->GetZoomMax();
        if (ZoomStage == 0)
        {
            Player->OnScroll(FInputActionValue(100.0f));
            if (Player->GetDesiredZoom() <= ZoomMin + 1.0f)
            {
                BotAssert(true, TEXT("zoom-min"), 1, 1);
                ZoomStage = 1;
            }
            return;
        }
        if (ZoomStage == 1)
        {
            Player->OnScroll(FInputActionValue(-100.0f));
            if (Player->GetDesiredZoom() >= ZoomMax - 1.0f)
            {
                BotAssert(true, TEXT("zoom-max"), 1, 1);
                ZoomStage = 2;
            }
            return;
        }
        // Boom converges to the target length via Tick interp.
        if (FMath::Abs(Player->GetCameraArmLength() - ZoomMax) < 15.0f)
        {
            BotAssert(true, TEXT("zoom-converge"), 1, 1);
            BotShot(TEXT("camera"));
            Step = EBotStep::MusicLeg;
            FlowLegStartTime = FPlatformTime::Seconds();
            return;
        }
        if (FPlatformTime::Seconds() - FlowLegStartTime > FlowTimeoutSec)
        {
            BotAssert(false, TEXT("zoom-converge"), 0, 1);
            Finish(false, TEXT("camera-timeout"));
        }
        return;
    }

    if (Step == EBotStep::CharList)
    {
        UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager();
        if (!Auth)
        {
            Finish(false, TEXT("no-auth"));
            return;
        }
        if (!bListReqSent)
        {
            Auth->OnCharacterListReceived.AddDynamic(this, &UDevModeBotBrain::HandleCharListReceived);
            Auth->OnCreateCharacterResponse.AddDynamic(this, &UDevModeBotBrain::HandleCreateResponse);
            Auth->OnDeleteCharacterResponse.AddDynamic(this, &UDevModeBotBrain::HandleDeleteResponse);
            FClientDataStruct ClientData = GameInstance->GetCurrentClientData();
            Auth->SendCharacterListRequest(ClientData);
            bListReqSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT character list requested"));
            return;
        }
        if (CharListCache.Num() > 0)
        {
            const int32 OwnId = GameInstance->GetCurrentCharacterID();
            bool bFound = false;
            for (const FLoginCharacterEntry& E : CharListCache)
            {
                if (E.CharacterId == OwnId) { bFound = true; break; }
            }
            BotAssert(bFound, TEXT("char-list-self"), bFound ? 1 : 0, 1);
            Step = EBotStep::CharCreate;
            FlowLegStartTime = Now;
            return;
        }
        if (Now - FlowLegStartTime > FlowTimeoutSec)
        {
            Finish(false, TEXT("char-list-timeout"));
        }
        return;
    }

    if (Step == EBotStep::CharCreate)
    {
        UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager();
        if (!Auth)
        {
            Finish(false, TEXT("no-auth"));
            return;
        }
        if (!CreateError.IsEmpty())
        {
            BotAssert(false, TEXT("char-created"), 0, 1);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT create failed: %s"), *CreateError);
            Finish(false, TEXT("create-failed"));
            return;
        }
        if (NewCharId > 0)
        {
            // Verify the newcomer shows up in a fresh list.
            if (!bListReqSent)
            {
                FClientDataStruct ClientData = GameInstance->GetCurrentClientData();
                Auth->SendCharacterListRequest(ClientData);
                bListReqSent = true;
                return;
            }
            for (const FLoginCharacterEntry& E : CharListCache)
            {
                if (E.CharacterId == NewCharId)
                {
                    BotAssert(true, TEXT("char-created"), NewCharId, NewCharId);
                    BotShot(TEXT("created"));
                    Step = EBotStep::CharDelete;
                    FlowLegStartTime = Now;
                    return;
                }
            }
            if (Now - FlowLegStartTime > FlowTimeoutSec)
            {
                BotAssert(false, TEXT("char-created"), 0, NewCharId);
                Finish(false, TEXT("create-verify-timeout"));
            }
            return;
        }
        if (CharName.IsEmpty())
        {
            if (NameLetterIdx > 4)
            {
                Finish(false, TEXT("names-exhausted"));
                return;
            }
            CharName = FString::Printf(TEXT("Test %c"), TCHAR('V' + NameLetterIdx));
        }
        if (!bCreateSent)
        {
            Auth->SendCreateCharacterRequest(CharName, TEXT("warrior"), TEXT("human"), TEXT("male"));
            bCreateSent = true;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT create requested '%s'"), *CharName);
        }
        if (Now - FlowLegStartTime > FlowTimeoutSec)
        {
            Finish(false, TEXT("create-timeout"));
        }
        return;
    }

    if (Step == EBotStep::CharDelete)
    {
        UAuthenticationManager* Auth = GameInstance->GetAuthenticationManager();
        if (!Auth)
        {
            Finish(false, TEXT("no-auth"));
            return;
        }
        if (!DeleteError.IsEmpty())
        {
            BotAssert(false, TEXT("char-deleted"), 0, 1);
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT delete failed: %s"), *DeleteError);
            Finish(false, TEXT("delete-failed"));
            return;
        }
        if (!bDeleteSent)
        {
            Auth->SendDeleteCharacterRequest(NewCharId);
            bDeleteSent = true;
            bListReqSent = false;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT delete requested id=%d"), NewCharId);
            return;
        }
        if (!bListReqSent)
        {
            FClientDataStruct ClientData = GameInstance->GetCurrentClientData();
            Auth->SendCharacterListRequest(ClientData);
            bListReqSent = true;
            return;
        }
        for (const FLoginCharacterEntry& E : CharListCache)
        {
            if (E.CharacterId == NewCharId)
            {
                if (Now - FlowLegStartTime > FlowTimeoutSec)
                {
                    BotAssert(false, TEXT("char-deleted"), 1, 0);
                    Finish(false, TEXT("delete-timeout"));
                }
                return;
            }
        }
        BotAssert(true, TEXT("char-deleted"), 1, 1);
        BotShot(TEXT("deleted"));
        Finish(Failures == 0, Failures == 0 ? TEXT("char-done") : TEXT("assert-failures"));
        return;
    }

    if (Step == EBotStep::BackToLogin)
    {
        // World timers die in the travel below: arm the PostLoadMap hook
        // and a core-ticker watchdog, then travel. The tombstone fires in
        // the callback, never in BrainTick.
        if (!PostLoadMapLoginHandle.IsValid())
        {
            PostLoadMapLoginHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
                this, &UDevModeBotBrain::HandlePostLoadMapLogin);
        }
        if (!bLoginTravelSent)
        {
            GameInstance->ReturnToLoginLevel();
            bLoginTravelSent = true;
            UE_LOG(LogTemp, Warning, TEXT("%s returning to login"), *TombstonePrefix);
            TWeakObjectPtr<UDevModeBotBrain> WeakThis(this);
            const double WatchStart = FPlatformTime::Seconds();
            FTSTicker::GetCoreTicker().RemoveTicker(LoginWatchdogHandle);
            LoginWatchdogHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([WeakThis, WatchStart](float)
                {
                    UDevModeBotBrain* Self = WeakThis.Get();
                    if (!Self || !Self->IsRunning()) { return false; }
                    if (FPlatformTime::Seconds() - WatchStart > 120.0)
                    {
                        Self->Finish(false, TEXT("login-travel-timeout"));
                    }
                    return Self->IsRunning();
                }),
                1.0f);
        }
        return;
    }

    ABasicMOB* Mob = TargetMob.Get();
    // Invariant: live target (repick above guarantees it; AssertKill and
    // GotoWaypoint returned). Defensive recover — never dereference stale.
    if (!IsValid(Mob) || Mob->GetMOBIsDead())
    {
        TargetMob.Reset();
        Step = EBotStep::GotoTarget;
        StepStartTime = Now;
        return;
    }
    const FVector MobAt = Mob->GetActorLocation();
    const float Range = Player->GetCurrentSkillRange();
    const float Dist = Dist2D(PlayerAt, MobAt);

    if (Step == EBotStep::GotoTarget)
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
                return;
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
                return;
            }
            // In range: real lock + real attack loop from here on.
            Player->OnAttackInput();
            bAttackStarted = Player->bIsAutoAttacking;
            CombatStartTime = Now;
            StepStartTime = Now;
            Step = EBotStep::Combat;
            UE_LOG(LogTemp, Warning, TEXT("DEVMODE_BOT combat vs '%s' (dist %.0f, attack-loop=%d)"),
                *Mob->GetMobName(), Dist, bAttackStarted ? 1 : 0);
            return;
        }

        if (Now - StepStartTime > GotoTimeoutSec)
        {
            Finish(false, TEXT("goto-timeout"));
        }
        return;
    }

    if (Step == EBotStep::Combat)
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
                return;
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
                return;
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
                return;
            }
        }
        Step = EBotStep::AssertKill;
        StepStartTime = Now;
        return;
    }
}

