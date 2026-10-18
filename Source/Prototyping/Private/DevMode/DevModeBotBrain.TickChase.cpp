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

        // NOTE: no global-census assert by design. On a live shared stand
        // the total alive count drifts for server-side reasons (respawns,
        // timed-champion expiry, janitor, other players), so an exact
        // `Alive == InitialAlive - Kills - 1` check is flaky and proves
        // nothing about OUR kills. Kill registration is pinned per-target
        // by mob-dead above plus the kills==TargetKills gate at Finish.

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
