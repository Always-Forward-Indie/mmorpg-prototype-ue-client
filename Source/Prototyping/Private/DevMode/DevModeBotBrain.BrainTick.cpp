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
        // Roundtrip leg-1 teardown race: ReturnToLoginLevel nulls the
        // player before the old world finishes tearing down, so one last
        // world-timer tick can land here after leg 1 already reported
        // done. That is the EXPECTED supervisor handoff, not a failure:
        // stay quiet and let the driver start leg 2 (its watchdogs still
        // bound a genuinely stuck run).
        if (!(bTravelRoundtripPhase && bRoundtripLeg1Done))
        {
            Finish(false, TEXT("lost-world-or-player"));
        }
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
        if (Tick_ExpectGround(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::AssertKill)
    {
        if (Tick_AssertKill(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::GotoWaypoint)
    {
        if (Tick_GotoWaypoint(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::GotoRendezvous)
    {
        if (Tick_GotoRendezvous(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::AssertVisible)
    {
        if (Tick_AssertVisible(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::GotoCorpse)
    {
        if (Tick_GotoCorpse(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::LootPickup)
    {
        if (Tick_LootPickup(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::ExpectDeath)
    {
        if (Tick_ExpectDeath(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::AssertDead)
    {
        if (Tick_AssertDead(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::RespawnWait)
    {
        if (Tick_RespawnWait(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::RecoverRevive)
    {
        if (Tick_RecoverRevive(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::VendorWalk)
    {
        if (Tick_VendorWalk(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::VendorSell)
    {
        if (Tick_VendorSell(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::VendorRepair)
    {
        if (Tick_VendorRepair(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::ChatExchange)
    {
        if (Tick_ChatExchange(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::QuestWalk)
    {
        if (Tick_QuestWalk(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::QuestAccept)
    {
        if (Tick_QuestAccept(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::QuestTurnin)
    {
        if (Tick_QuestTurnin(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::SkillWalk)
    {
        if (Tick_SkillWalk(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::SkillLearn)
    {
        if (Tick_SkillLearn(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::SkillCast)
    {
        if (Tick_SkillCast(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::SkillBuff)
    {
        if (Tick_SkillBuff(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::EquipOn)
    {
        if (Tick_EquipOn(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::EquipOff)
    {
        if (Tick_EquipOff(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::LevelWait)
    {
        if (Tick_LevelWait(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::TitleLeg)
    {
        if (Tick_TitleLeg(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::EmoteLeg)
    {
        if (Tick_EmoteLeg(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::LocaleLeg)
    {
        if (Tick_LocaleLeg(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::MusicLeg)
    {
        if (Tick_MusicLeg(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::CameraLeg)
    {
        if (Tick_CameraLeg(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::CharList)
    {
        if (Tick_CharList(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::CharCreate)
    {
        if (Tick_CharCreate(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::CharDelete)
    {
        if (Tick_CharDelete(World, Player, Now, PlayerAt)) { return; }
    }
    if (Step == EBotStep::BackToLogin)
    {
        if (Tick_BackToLogin(World, Player, Now, PlayerAt)) { return; }
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
        if (Tick_GotoTarget(World, Player, Now, PlayerAt, Mob, MobAt, Range, Dist)) { return; }
    }
    if (Step == EBotStep::Combat)
    {
        if (Tick_Combat(World, Player, Now, PlayerAt, Mob, MobAt, Range, Dist)) { return; }
    }
}
