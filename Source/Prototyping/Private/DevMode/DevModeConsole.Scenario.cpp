#include "DevMode/DevModeConsoleCommands.h"
#include "Prototyping.h"
#include "DevMode/DevModeBotBrain.h"
#include "MyGameInstance.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Teleport/TeleportManager.h"
#include "DevMode/DevModeDataProvider.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HighResScreenshot.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

void UDevModeConsoleCommands::Cmd_Scenario(const TArray<FString>& Args) const
{
    if (Args.Num() < 1)
    {
        UE_LOG(LogGameplay, Warning, TEXT("Usage: devmode.scenario <combat|reset|teleport|full_sweep|soak|toast_verify|bot_combat_patrol|bot_loot_pickup|bot_death_respawn|bot_harvest_run|bot_periphery>"));
        return;
    }
    if (!GameInstance) return;

    const FString Name = Args[0].ToLower();
    TArray<FString> NoArgs;

    if (Name == TEXT("combat"))
    {
        Cmd_ReloadMobs(NoArgs);

        UWorld* World = GameInstance->GetWorld();
        ABasicMOB* First = nullptr;
        if (World)
        {
            for (TActorIterator<ABasicMOB> It(World); It; ++It)
            {
                First = *It;
                break;
            }
        }
        if (First && GameInstance->Player)
        {
            const FVector At = First->GetActorLocation() + FVector(-300.0f, 0.0f, 0.0f);
            GameInstance->Player->SetActorLocation(At);
            UE_LOG(LogGameplay, Verbose, TEXT("devmode.scenario combat: near '%s' at (%.0f,%.0f,%.0f)"),
                *First->GetMobName(), At.X, At.Y, At.Z);
        }
        else
        {
            UE_LOG(LogGameplay, Warning, TEXT("devmode.scenario combat: no mob/player found"));
        }
    }
    else if (Name == TEXT("reset"))
    {
        Cmd_ReloadMobs(NoArgs);
        Cmd_ReloadInventory(NoArgs);
        if (GameInstance->Player)
        {
            GameInstance->Player->SetActorLocation(FVector(0.0f, 0.0f, 90.0f));
        }
        UE_LOG(LogGameplay, Verbose, TEXT("devmode.scenario reset: world re-seeded, player at spawn"));
    }
    else if (Name == TEXT("teleport"))
    {
        // AAA cover-transition self-test: fade -> covered snap -> reveal,
        // then assert arrival and emit the bot tombstone.
        if (!GameInstance->Player)
        {
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT_RESULT scenario=teleport FAIL (no player)"));
        }
        else if (UTeleportManager* TM = GameInstance->GetTeleportManager())
        {
            ABasicPlayer* P = GameInstance->Player;
            const FVector From = P->GetActorLocation();
            const FVector Dest = From + FVector(2000.0f, 0.0f, 300.0f);
            TM->SetWorldContext(GameInstance->GetWorld());
            TM->ApplyLocalTeleport(P, Dest, P->GetActorRotation().Yaw, TEXT("devmode-scenario"));
            if (UWorld* World = GameInstance->GetWorld())
            {
                TWeakObjectPtr<ABasicPlayer> WeakP(P);
                FTimerHandle TeleportCheckHandle;
                World->GetTimerManager().SetTimer(TeleportCheckHandle, [this, WeakP, From, Dest]()
                {
                    const bool bPass = WeakP.IsValid()
                        && FVector::Dist2D(WeakP->GetActorLocation(), Dest) < 150.0f
                        && !WeakP->IsTeleportCoverActive();
                    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT_RESULT scenario=teleport %s (from (%.0f,%.0f) to (%.0f,%.0f))"),
                        bPass ? TEXT("PASS") : TEXT("FAIL"), From.X, From.Y,
                        WeakP.IsValid() ? WeakP->GetActorLocation().X : 0.0f,
                        WeakP.IsValid() ? WeakP->GetActorLocation().Y : 0.0f);
                    if (GameInstance && GameInstance->bDevModeAutoQuit)
                    {
                        GameInstance->bDevModeAutoQuit = false;
                        GameInstance->DevModeAutoScenario.Empty();
                        if (UWorld* QuitWorld = GameInstance->GetWorld())
                        {
                            if (GEngine)
                            {
                                GEngine->Exec(QuitWorld, TEXT("quit"));
                            }
                        }
                    }
                }, 1.5f, false);
            }
        }
        else
        {
            UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_BOT_RESULT scenario=teleport FAIL (no TeleportManager)"));
        }
    }
    else if (Name == TEXT("full_sweep"))
    {
        // Golden offline sweep, two phases (see RunSweepPhase1/2).
        RunSweepPhase1();
        if (GameInstance)
        {
            GameInstance->ScheduleSweepPhase2(1.5f);
        }
    }
    else if (Name == TEXT("soak"))
    {
        // L4 soak loop: sweep churn cycles + mem log. CLI: -soak-iters=N
        // (0 = until killed), -soak-period=F (gap seconds between cycles).
        StartSoakLoop();
    }
    else if (Name == TEXT("bot_combat_patrol")
        || Name == TEXT("bot_loot_pickup")
        || Name == TEXT("bot_death_respawn")
        || Name == TEXT("bot_harvest_run")
        || Name == TEXT("bot_periphery"))
    {
        // Auto-run bot names strip the "bot_" prefix (brain scenario names
        // are loot_pickup / death_respawn / combat_patrol / harvest_run /
        // periphery).
        FString BrainScenario = Name;
        BrainScenario.RemoveFromStart(TEXT("bot_"));
        StartBotBrain(BrainScenario);
    }
    else if (Name == TEXT("toast_verify"))
    {
        // Toast stack self-test: fires the mix, asserts visibility, quits.
        Cmd_ToastVerify(NoArgs);
        if (GameInstance && GameInstance->bDevModeAutoQuit)
        {
            GameInstance->bDevModeAutoQuit = false;
            GameInstance->DevModeAutoScenario.Empty();
            if (UWorld* World = GameInstance->GetWorld())
            {
                if (GEngine)
                {
                    GEngine->Exec(World, TEXT("quit"));
                }
            }
        }
    }
    else if (Name == TEXT("coalesce_verify"))
    {
        // Coalesce self-test: 8 identical receipts collapse to one, quits.
        // Runs on an idle stack — never chain after toast_verify.
        Cmd_CoalesceVerify(NoArgs);
        if (GameInstance && GameInstance->bDevModeAutoQuit)
        {
            GameInstance->bDevModeAutoQuit = false;
            GameInstance->DevModeAutoScenario.Empty();
            if (UWorld* World = GameInstance->GetWorld())
            {
                if (GEngine)
                {
                    GEngine->Exec(World, TEXT("quit"));
                }
            }
        }
    }
    else if (Name == TEXT("bot_vendor_repair"))
    {
        // Economy shops need the server round-trip — live only.
        UE_LOG(LogGameplay, Warning, TEXT("devmode.scenario: bot_vendor_repair is live-only (use -bot-live=live_vendor_repair with dev servers)"));
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("devmode.scenario: unknown '%s' (use combat|reset|teleport|full_sweep|soak|toast_verify|bot_combat_patrol|bot_loot_pickup|bot_death_respawn|bot_harvest_run|bot_periphery)"), *Args[0]);
    }
}

void UDevModeConsoleCommands::StartBotBrain(const FString& ScenarioName) const
{
    if (!GameInstance) return;
    if (GameInstance->DevModeBotBrain && GameInstance->DevModeBotBrain->IsRunning())
    {
        UE_LOG(LogGameplay, Warning, TEXT("devmode.bot: brain already running"));
        return;
    }
    if (!GameInstance->DevModeBotBrain)
    {
        GameInstance->DevModeBotBrain = NewObject<UDevModeBotBrain>(GameInstance);
    }
    UDevModeBotBrain* Brain = GameInstance->DevModeBotBrain;
    // Reset to offline defaults, then apply the scenario profile. One brain,
    // different flags — same rule as the live driver below.
    Brain->ScenarioName = ScenarioName;
    Brain->TombstonePrefix = TEXT("DEVMODE_BOT");
    Brain->bTeleportToSpawn = true;
    Brain->bFreezeMobs = true;
    Brain->bStubHits = true;
    Brain->bAutoQuitAtFinish = false;
    Brain->bDirectMove = false;
    Brain->bRendezvousMode = false;
    Brain->bLootPhase = false;
    Brain->bStubLoot = true;
    Brain->bDeathPhase = false;
    Brain->bStubDeath = true;
    Brain->bPassiveCombat = false;
    Brain->bHarvestPhase = false;
    Brain->bStubHarvest = true;
    Brain->bVendorPhase = false;
    Brain->bPeripheryPhase = false;
    Brain->bCharFlowPhase = false;
    Brain->bBackToLoginPhase = false;
    Brain->TargetKills = 2;
    Brain->GlobalTimeoutSec = 120.0f;
    if (ScenarioName.Equals(TEXT("loot_pickup"), ESearchCase::IgnoreCase))
    {
        Brain->TargetKills = 1;
        Brain->bLootPhase = true;
        Brain->bStubLoot = true;
    }
    else if (ScenarioName.Equals(TEXT("death_respawn"), ESearchCase::IgnoreCase))
    {
        Brain->TargetKills = 0;
        Brain->bDeathPhase = true;
        Brain->bStubDeath = true;
    }
    else if (ScenarioName.Equals(TEXT("harvest_run"), ESearchCase::IgnoreCase))
    {
        Brain->TargetKills = 1;
        Brain->bLootPhase = true;
        Brain->bStubLoot = true;
        Brain->bHarvestPhase = true;
        Brain->bStubHarvest = true;
    }
    else if (ScenarioName.Equals(TEXT("periphery"), ESearchCase::IgnoreCase))
    {
        Brain->TargetKills = 0;
        Brain->bPeripheryPhase = true;
    }
    Brain->Start(GameInstance);
}

void UDevModeConsoleCommands::RunScenario(const FString& ScenarioName) const
{
    TArray<FString> Args = { ScenarioName };
    Cmd_Scenario(Args);
}

void UDevModeConsoleCommands::ReloadMobsNow() const
{
    TArray<FString> NoArgs;
    Cmd_ReloadMobs(NoArgs);
}

void UDevModeConsoleCommands::RunSweepPhase1() const
{
    // Phase 1: reset to seeded state. Actor Destroy() is deferred to end of
    // frame, so nothing is asserted here — phase 2 runs after cleanup.
    SweepFailures = 0;
    PerfReset();
    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_SWEEP_BEGIN full_sweep"));
    TArray<FString> NoArgs;
    Cmd_ReloadMobs(NoArgs);
    Cmd_ReloadInventory(NoArgs);
    if (GameInstance && GameInstance->Player)
    {
        FString SweepSnapError;
        GameInstance->Player->SnapToAuthoritativePosition(FVector(0.0f, 0.0f, 90.0f),
            GameInstance->Player->GetActorRotation().Yaw, SweepSnapError);
        GameInstance->Player->SnapToGround();
    }
    SweepShot(TEXT("phase1-seeded"));
    // NOTE: no PerfMark here — reset and mark would land on the same frame
    // (0 frames / 0.00s window, proven noisy). Real windows are phases 2-3.
}

void UDevModeConsoleCommands::RunSweepPhase2() const
{
    // Phase 2: the phase-1 destroyed actors are gone, the world matches dev_*.json.
    // The phase-1 spawn spot had 1.5s to settle — grounded assert covers it.
    ExpectGrounded();
    // Kills two mobs by explicit UID (mobUniqueID from dev_mobs.json), then
    // clears the corpses — Destroy() is deferred, so respawns wait for phase 3.
    TArray<FString> NoArgs;
    ExpectValue(TEXT("mobs"), TEXT("eq"), 3);
    ExpectValue(TEXT("inv"), TEXT("ge"), 5);

    TArray<FString> TeleArgs = { TEXT("300"), TEXT("200"), TEXT("90") };
    Cmd_Teleport(TeleArgs);
    TArray<FString> KillA = { TEXT("9901") };
    Cmd_KillMob(KillA);
    ExpectValue(TEXT("mobs"), TEXT("eq"), 2);
    TArray<FString> KillB = { TEXT("9902") };
    Cmd_KillMob(KillB);
    ExpectValue(TEXT("mobs"), TEXT("eq"), 1);
    Cmd_ClearCorpses(NoArgs);
    SweepShot(TEXT("phase2-corpses"));
    PerfMark(TEXT("phase2-corpses"));

    if (GameInstance)
    {
        GameInstance->ScheduleSweepPhase3(1.5f);
    }
}

void UDevModeConsoleCommands::RunSweepPhase3() const
{
    // Phase 3: corpses are gone, UID tags are free — respawn and verify.
    // The phase-2 teleport spot had 1.5s to settle — grounded assert covers it.
    ExpectGrounded();
    TArray<FString> NoArgs;
    TArray<FString> SpawnA = { TEXT("1") };
    Cmd_SpawnMob(SpawnA);
    TArray<FString> SpawnB = { TEXT("2") };
    Cmd_SpawnMob(SpawnB);
    ExpectValue(TEXT("mobs"), TEXT("eq"), 3);

    TArray<FString> LowHP = { TEXT("1") };
    Cmd_SetPlayerHP(LowHP);
    ExpectValue(TEXT("hp"), TEXT("eq"), 1);
    TArray<FString> FullHP = { TEXT("1000") };
    Cmd_SetPlayerHP(FullHP);
    ExpectValue(TEXT("hp"), TEXT("eq"), 1000);

    Cmd_ListMobs(NoArgs);
    SweepShot(TEXT("phase3-result"));
    PerfMark(TEXT("phase3-result"));
    PerfStop();
    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_SWEEP_RESULT %s failures=%d"),
        SweepFailures == 0 ? TEXT("PASS") : TEXT("FAIL"), SweepFailures);
}

void UDevModeConsoleCommands::StartSoakLoop() const
{
    // Manual console call while looping stops the loop (clean SOAK_RESULT).
    if (bSoakLoop)
    {
        bSoakLoop = false;
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_SOAK_RESULT %s iters=%d"),
            SweepFailures == 0 ? TEXT("PASS") : TEXT("FAIL"), SoakIter);
        return;
    }
    int32 Iters = 0;
    float Period = 5.0f;
    FParse::Value(FCommandLine::Get(), TEXT("soak-iters="), Iters);
    FParse::Value(FCommandLine::Get(), TEXT("soak-period="), Period);
    SoakMaxIters = Iters;
    SoakPeriodSec = (Period > 0.5f) ? Period : 5.0f;
    SoakIter = 0;
    SweepFailures = 0;
    bSoakLoop = true;
    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_SOAK_BEGIN iters=%d period=%.1fs"),
        SoakMaxIters, SoakPeriodSec);
    RunSoakCycle();
}

void UDevModeConsoleCommands::RunSoakCycle() const
{
    if (!bSoakLoop) { return; }
    // One sweep churn cycle (reset/kill/respawn/HP); phase 3 chains back
    // here via SweepStep3 -> SoakContinue -> ScheduleSoakCycle.
    RunSweepPhase1();
    if (GameInstance)
    {
        GameInstance->ScheduleSweepPhase2(1.5f);
    }
}

void UDevModeConsoleCommands::ScheduleSoakCycle() const
{
    if (GameInstance)
    {
        GameInstance->ScheduleSoakCycle(SoakPeriodSec);
    }
}

bool UDevModeConsoleCommands::SoakContinue() const
{
    if (!bSoakLoop) { return false; }
    ++SoakIter;
    // Memory watch: resident + peak (MB). Growth across iters = leak hunt.
    const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
    const double UsedMB = (double)Mem.UsedPhysical / (1024.0 * 1024.0);
    const double PeakMB = (double)Mem.PeakUsedPhysical / (1024.0 * 1024.0);
    UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_SOAK iter=%d memMB=%.0f peakMB=%.0f"),
        SoakIter, UsedMB, PeakMB);
    if (SoakMaxIters > 0 && SoakIter >= SoakMaxIters)
    {
        bSoakLoop = false;
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_SOAK_RESULT %s iters=%d"),
            SweepFailures == 0 ? TEXT("PASS") : TEXT("FAIL"), SoakIter);
        return false;
    }
    return true;
}

// ============================================================================
// devmode.expect <Key> <Op> <Value>  (Keys: hp|mobs|inv|gold; Ops: eq|ne|gt|lt|ge|le)
// ============================================================================
