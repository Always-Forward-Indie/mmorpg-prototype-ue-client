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

bool UDevModeBotBrain::Tick_ExpectDeath(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
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
            return true;
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
                return true;
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
                    return true;
                }
                ChaseCheckTime = Now;
                ChaseCheckDist = KillerDist;
            }
            if (Now - DeathLegStartTime > DeathTimeoutSec)
            {
                Finish(false, TEXT("killer-unreachable"));
            }
            return true;
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
            return true;
        }
        if (Now - DeathLegStartTime > DeathTimeoutSec)
        {
            Finish(false, FString::Printf(TEXT("no-death hp=%d dist=%.0f"), NowHP, KillerDist));
        }
        // Still alive: keep standing here; the top-of-tick dead check
        // redirects to AssertDead the moment a server hit lands.
        return true;
}

bool UDevModeBotBrain::Tick_AssertDead(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
{
        // Entered only while dead (top-of-tick redirect or stub handoff).
        if (!Player->GetIsDead())
        {
            Finish(false, TEXT("death-not-registered"));
            return true;
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
            return true;
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
        return true;
}

bool UDevModeBotBrain::Tick_RespawnWait(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
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
            return true;
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
            return true;
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
        return true;
}

bool UDevModeBotBrain::Tick_RecoverRevive(UWorld* World, ABasicPlayer* Player, double Now, const FVector& PlayerAt)
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
            return true;
        }
        if (FPlatformTime::Seconds() - RespawnLegStartTime > RespawnTimeoutSec)
        {
            Finish(false, TEXT("spawned-dead"));
        }
        return true;
}
