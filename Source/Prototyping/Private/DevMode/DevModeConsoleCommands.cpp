#include "DevMode/DevModeConsoleCommands.h"
#include "MyGameInstance.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MOBMovementComponent.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "DevMode/DevModeDataProvider.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ============================================================================
void UDevModeConsoleCommands::RegisterCommands(UMyGameInstance* InGameInstance)
{
    GameInstance = InGameInstance;
    IConsoleManager& CM = IConsoleManager::Get();

    // Capture a weak pointer so that any deferred/in-flight delegate invocation
    // after the object has been GC'd or unregistered will safely no-op.
    // The delegate is built directly (no intermediate TFunction wrapper) to avoid
    // a double-indirection whose inner TFunction vtable can be hit during engine
    // teardown when UnregisterConsoleObject defers the delegate's destruction.
    TWeakObjectPtr<UDevModeConsoleCommands> WeakThis(this);

    auto Reg = [&](const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithArgsDelegate& Delegate)
    {
        CM.RegisterConsoleCommand(Name, Help, Delegate, ECVF_Cheat);
        RegisteredCommandNames.Add(Name);
    };

    Reg(TEXT("devmode.spawnmob"),
        TEXT("DevMode: spawn a mob by mobID from dev_mobs.json. Usage: devmode.spawnmob <MobID>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_SpawnMob(A); }
        }));

    Reg(TEXT("devmode.setcombatstate"),
        TEXT("DevMode: set combat state on a mob. Usage: devmode.setcombatstate <MobUID> <State>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_SetCombatState(A); }
        }));

    Reg(TEXT("devmode.teleport"),
        TEXT("DevMode: teleport local player. Usage: devmode.teleport <X> <Y> <Z>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_Teleport(A); }
        }));

    Reg(TEXT("devmode.setplayerhp"),
        TEXT("DevMode: set local player current HP. Usage: devmode.setplayerhp <Value>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_SetPlayerHP(A); }
        }));

    Reg(TEXT("devmode.reloadmobs"),
        TEXT("DevMode: destroy all dev mobs and re-spawn from JSON."),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_ReloadMobs(A); }
        }));

    Reg(TEXT("devmode.reloadinventory"),
        TEXT("DevMode: clear and re-populate inventory from JSON."),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_ReloadInventory(A); }
        }));

    Reg(TEXT("devmode.listmobs"),
        TEXT("DevMode: print all ABasicMOB actors to log."),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_ListMobs(A); }
        }));

    Reg(TEXT("devmode.scenario"),
        TEXT("DevMode: one-shot QA flows. Usage: devmode.scenario <combat|reset|full_sweep|bot_combat_patrol>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_Scenario(A); }
        }));

    Reg(TEXT("devmode.bot"),
        TEXT("DevMode: start the L1 action bot. Usage: devmode.bot <combat_patrol|loot_pickup|death_respawn>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get())
            {
                if (A.Num() < 1
                    || (!A[0].Equals(TEXT("combat_patrol"), ESearchCase::IgnoreCase)
                        && !A[0].Equals(TEXT("loot_pickup"), ESearchCase::IgnoreCase)
                        && !A[0].Equals(TEXT("death_respawn"), ESearchCase::IgnoreCase)))
                {
                    UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.bot <combat_patrol|loot_pickup|death_respawn>"));
                    return;
                }
                Cmd->StartBotBrain(A[0].ToLower());
            }
        }));

    Reg(TEXT("devmode.expect"),
        TEXT("DevMode: assert live state. Usage: devmode.expect <hp|mobs|inv|gold> <eq|ne|gt|lt|ge|le> <Value>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_Expect(A); }
        }));

    Reg(TEXT("devmode.setmobhp"),
        TEXT("DevMode: set mob HP (<=0 also runs Die()). Usage: devmode.setmobhp <MobUID> <Value>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_SetMobHP(A); }
        }));

    Reg(TEXT("devmode.killmob"),
        TEXT("DevMode: run Die() on a mob. Usage: devmode.killmob [MobUID] (first alive if omitted)"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_KillMob(A); }
        }));

    Reg(TEXT("devmode.clearcorpses"),
        TEXT("DevMode: destroy all dead mob actors (frees their UID tags for respawn)."),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_ClearCorpses(A); }
        }));

    UE_LOG(LogTemp, Log, TEXT("DevMode: %d console commands registered"), RegisteredCommandNames.Num());
}

void UDevModeConsoleCommands::BeginDestroy()
{
    // GC is about to free this UObject. Unregister console commands now so
    // we never leave dangling IConsoleObject* alive after our memory is gone.
    // This covers PIE stop, where GC runs before GameInstance::Shutdown().
    UnregisterCommands();
    Super::BeginDestroy();
}

void UDevModeConsoleCommands::UnregisterCommands()
{
    if (RegisteredCommandNames.Num() == 0)
    {
        return;
    }

    // Nullify GameInstance first so that any in-flight delegate invocation
    // hits the null-check guard and bails out safely.
    GameInstance = nullptr;

    // Unregister by name so the engine synchronously deletes the
    // FConsoleCommandWithArgs object (and its stored delegate/lambda) right
    // now, while the UObject system is still intact.  Holding raw
    // IConsoleObject* and deferring this until engine teardown causes the
    // TWeakObjectPtr destructor inside the lambda to run against a freed
    // UObject tracking table, which produces the 0xC0000005 crash.
    IConsoleManager& CM = IConsoleManager::Get();
    for (const FString& Name : RegisteredCommandNames)
    {
        CM.UnregisterConsoleObject(*Name, /*bKeepState=*/false);
    }
    RegisteredCommandNames.Empty();
}

// ============================================================================
// devmode.spawnmob <MobID>
// ============================================================================
void UDevModeConsoleCommands::Cmd_SpawnMob(const TArray<FString>& Args) const
{
    if (!GameInstance || !GameInstance->MOBManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.spawnmob: MOBManager not available"));
        return;
    }
    if (Args.Num() < 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.spawnmob <MobID>"));
        return;
    }

    const int32 TargetMobID = FCString::Atoi(*Args[0]);

    // Re-read the JSON to find the matching entry
    UDevModeDataProvider* Provider = NewObject<UDevModeDataProvider>(GameInstance);
    if (!Provider) return;

    // We need Config � get it from GameInstance
    Provider->Initialize(GameInstance, GameInstance->DevModeConfig);

    // Read JSON manually here to find just one entry
    const FString AbsPath = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), GameInstance->DevModeConfig.MobDataJsonPath));
    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *AbsPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.spawnmob: Could not read mob JSON"));
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TArray<TSharedPtr<FJsonValue>>* MobsArr = nullptr;
    if (!Root->TryGetArrayField(TEXT("mobs"), MobsArr)) return;

    for (const TSharedPtr<FJsonValue>& Entry : *MobsArr)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Entry->TryGetObject(ObjPtr)) continue;
        if ((*ObjPtr)->GetIntegerField(TEXT("mobID")) != TargetMobID) continue;

        FMOBStruct MobData;
        // Use ParseMobEntry via PopulateMobs path
        // Quick direct parse for console command
        MobData.mobID             = TargetMobID;
        MobData.mobUniqueID       = (*ObjPtr)->GetStringField(TEXT("mobUniqueID"));
        MobData.mobName           = (*ObjPtr)->GetStringField(TEXT("mobName"));
        MobData.mobSlug           = (*ObjPtr)->GetStringField(TEXT("mobSlug"));
        MobData.mobLevel          = (*ObjPtr)->GetIntegerField(TEXT("mobLevel"));
        MobData.mobCurrentHealth  = (*ObjPtr)->GetIntegerField(TEXT("mobCurrentHealth"));
        MobData.bIsAggressive     = (*ObjPtr)->GetBoolField(TEXT("bIsAggressive"));

        const TSharedPtr<FJsonObject>* PosObj = nullptr;
        if ((*ObjPtr)->TryGetObjectField(TEXT("mobPosition"), PosObj) && PosObj)
        {
            MobData.mobPosition.positionX = (*PosObj)->GetNumberField(TEXT("positionX"));
            MobData.mobPosition.positionY = (*PosObj)->GetNumberField(TEXT("positionY"));
            MobData.mobPosition.positionZ = (*PosObj)->GetNumberField(TEXT("positionZ"));
            MobData.mobPosition.rotationZ = (*PosObj)->GetNumberField(TEXT("rotationZ"));
        }

        GameInstance->MOBManager->SpawnMOB(MobData);
        UE_LOG(LogTemp, Log, TEXT("devmode.spawnmob: Spawned mob id=%d '%s'"), TargetMobID, *MobData.mobName);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("devmode.spawnmob: No mob with mobID=%d in JSON"), TargetMobID);
}

// ============================================================================
// devmode.setcombatstate <MobUID> <State>
// ============================================================================
void UDevModeConsoleCommands::Cmd_SetCombatState(const TArray<FString>& Args) const
{
    if (Args.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.setcombatstate <MobUID> <State>"));
        UE_LOG(LogTemp, Warning, TEXT("  States: 0=PATROL 1=CHASE 2=PREPARING_ATTACK 3=ATTACKING 4=COOLDOWN 5=RETURN 6=EVADING 7=FLEE"));
        return;
    }

    if (!GameInstance || !GameInstance->MOBManager) return;

    const int32 MobUID    = FCString::Atoi(*Args[0]);
    const int32 NewState  = FCString::Atoi(*Args[1]);

    AActor* MobActor = GameInstance->MOBManager->FindMobActor(MobUID);
    if (!MobActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.setcombatstate: Mob UID=%d not found"), MobUID);
        return;
    }

    ABasicMOB* Mob = Cast<ABasicMOB>(MobActor);
    if (!Mob || !Mob->MOBMovementComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.setcombatstate: No MOBMovementComponent on mob UID=%d"), MobUID);
        return;
    }

    Mob->MOBMovementComponent->SetCombatState(NewState);
    UE_LOG(LogTemp, Log, TEXT("devmode.setcombatstate: Mob UID=%d -> state %d"), MobUID, NewState);
}

// ============================================================================
// devmode.teleport <X> <Y> <Z>
// ============================================================================
void UDevModeConsoleCommands::Cmd_Teleport(const TArray<FString>& Args) const
{
    if (Args.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.teleport <X> <Y> <Z>"));
        return;
    }
    if (!GameInstance || !GameInstance->Player) return;

    const float X = FCString::Atof(*Args[0]);
    const float Y = FCString::Atof(*Args[1]);
    const float Z = FCString::Atof(*Args[2]);

    GameInstance->Player->SetActorLocation(FVector(X, Y, Z));
    UE_LOG(LogTemp, Log, TEXT("devmode.teleport: Player teleported to (%.0f, %.0f, %.0f)"), X, Y, Z);
}

// ============================================================================
// devmode.setplayerhp <Value>
// ============================================================================
void UDevModeConsoleCommands::Cmd_SetPlayerHP(const TArray<FString>& Args) const
{
    if (Args.Num() < 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.setplayerhp <Value>"));
        return;
    }
    if (!GameInstance || !GameInstance->Player) return;

    const int32 NewHP = FCString::Atoi(*Args[0]);
    GameInstance->Player->SetPlayerCurrentHPPoints(NewHP);
    UE_LOG(LogTemp, Log, TEXT("devmode.setplayerhp: Player HP set to %d"), NewHP);
}

// ============================================================================
// devmode.reloadmobs
// ============================================================================
void UDevModeConsoleCommands::Cmd_ReloadMobs(const TArray<FString>& Args) const
{
    if (!GameInstance || !GameInstance->MOBManager) return;

    // Destroy all existing BasicMOB actors in the world
    UWorld* World = GameInstance->GetWorld();
    if (!World) return;

    TArray<AActor*> ToDestroy;
    for (TActorIterator<ABasicMOB> It(World); It; ++It)
    {
        ToDestroy.Add(*It);
    }
    for (AActor* A : ToDestroy)
    {
        A->Destroy();
    }
    UE_LOG(LogTemp, Log, TEXT("devmode.reloadmobs: Destroyed %d mobs"), ToDestroy.Num());

    UDevModeDataProvider* Provider = NewObject<UDevModeDataProvider>(GameInstance);
    if (Provider)
    {
        Provider->Initialize(GameInstance, GameInstance->DevModeConfig);
        Provider->PopulateMobs(GameInstance->MOBManager);
    }
}

// ============================================================================
// devmode.reloadinventory
// ============================================================================
void UDevModeConsoleCommands::Cmd_ReloadInventory(const TArray<FString>& Args) const
{
    if (!GameInstance || !GameInstance->InventoryManager) return;

    UDevModeDataProvider* Provider = NewObject<UDevModeDataProvider>(GameInstance);
    if (Provider)
    {
        Provider->Initialize(GameInstance, GameInstance->DevModeConfig);
        Provider->PopulateInventory(GameInstance->InventoryManager,
                                    GameInstance->GetCurrentCharacterID());
    }
}

// ============================================================================
// devmode.listmobs
// ============================================================================
void UDevModeConsoleCommands::Cmd_ListMobs(const TArray<FString>& Args) const
{
    UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.listmobs: No world"));
        return;
    }

    int32 Count = 0;
    for (TActorIterator<ABasicMOB> It(World); It; ++It)
    {
        ABasicMOB* Mob = *It;
        const FVector Loc = Mob->GetActorLocation();
        const int32 State = Mob->MOBMovementComponent
            ? Mob->MOBMovementComponent->GetCombatState() : -1;
        UE_LOG(LogTemp, Log,
            TEXT("  [%d] %s  uid=%s  loc=(%.0f,%.0f,%.0f)  combatState=%d"),
            Count, *Mob->GetMobName(), *Mob->GetMOBUId(),
            Loc.X, Loc.Y, Loc.Z, State);
        ++Count;
    }
    UE_LOG(LogTemp, Log, TEXT("devmode.listmobs: Total %d mobs"), Count);
}

// ============================================================================
// devmode.scenario <combat|reset>
// One-shot QA flows for testers/players (no server needed).
//   combat - reload mobs, teleport player near the first mob
//   reset  - reload mobs + inventory, teleport player back to dev spawn
// ============================================================================
void UDevModeConsoleCommands::Cmd_Scenario(const TArray<FString>& Args) const
{
    if (Args.Num() < 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.scenario <combat|reset|full_sweep|bot_combat_patrol|bot_loot_pickup|bot_death_respawn>"));
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
            UE_LOG(LogTemp, Log, TEXT("devmode.scenario combat: near '%s' at (%.0f,%.0f,%.0f)"),
                *First->GetMobName(), At.X, At.Y, At.Z);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("devmode.scenario combat: no mob/player found"));
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
        UE_LOG(LogTemp, Log, TEXT("devmode.scenario reset: world re-seeded, player at spawn"));
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
    else if (Name == TEXT("bot_combat_patrol")
        || Name == TEXT("bot_loot_pickup")
        || Name == TEXT("bot_death_respawn"))
    {
        // Auto-run bot names strip the "bot_" prefix (brain scenario names
        // are loot_pickup / death_respawn / combat_patrol).
        FString BrainScenario = Name;
        BrainScenario.RemoveFromStart(TEXT("bot_"));
        StartBotBrain(BrainScenario);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.scenario: unknown '%s' (use combat|reset|full_sweep|bot_combat_patrol|bot_loot_pickup|bot_death_respawn)"), *Args[0]);
    }
}

void UDevModeConsoleCommands::StartBotBrain(const FString& ScenarioName) const
{
    if (!GameInstance) return;
    if (GameInstance->DevModeBotBrain && GameInstance->DevModeBotBrain->IsRunning())
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.bot: brain already running"));
        return;
    }
    if (!GameInstance->DevModeBotBrain)
    {
        GameInstance->DevModeBotBrain = NewObject<UDevModeBotBrain>(GameInstance);
    }
    UDevModeBotBrain* Brain = GameInstance->DevModeBotBrain;
    // Reset to offline defaults, then apply the scenario profile. One brain,
    // different flags тАФ same rule as the live driver below.
    Brain->ScenarioName = ScenarioName;
    Brain->TombstonePrefix = TEXT("DEVMODE_BOT");
    Brain->bTeleportToSpawn = true;
    Brain->bStubHits = true;
    Brain->bAutoQuitAtFinish = false;
    Brain->bDirectMove = false;
    Brain->bRendezvousMode = false;
    Brain->bLootPhase = false;
    Brain->bStubLoot = true;
    Brain->bDeathPhase = false;
    Brain->bStubDeath = true;
    Brain->bPassiveCombat = false;
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
    Brain->Start(GameInstance);
}

void UDevModeConsoleCommands::RunScenario(const FString& ScenarioName) const
{
    TArray<FString> Args = { ScenarioName };
    Cmd_Scenario(Args);
}

void UDevModeConsoleCommands::RunSweepPhase1() const
{
    // Phase 1: reset to seeded state. Actor Destroy() is deferred to end of
    // frame, so nothing is asserted here тАФ phase 2 runs after cleanup.
    SweepFailures = 0;
    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_SWEEP_BEGIN full_sweep"));
    TArray<FString> NoArgs;
    Cmd_ReloadMobs(NoArgs);
    Cmd_ReloadInventory(NoArgs);
    if (GameInstance && GameInstance->Player)
    {
        GameInstance->Player->SetActorLocation(FVector(0.0f, 0.0f, 90.0f));
        SnapPlayerToGround();
    }
    SweepShot(TEXT("phase1-seeded"));
}

void UDevModeConsoleCommands::RunSweepPhase2() const
{
    // Phase 2: the phase-1 destroyed actors are gone, the world matches dev_*.json.
    // Kills two mobs by explicit UID (mobUniqueID from dev_mobs.json), then
    // clears the corpses тАФ Destroy() is deferred, so respawns wait for phase 3.
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

    if (GameInstance)
    {
        GameInstance->ScheduleSweepPhase3(1.5f);
    }
}

void UDevModeConsoleCommands::RunSweepPhase3() const
{
    // Phase 3: corpses are gone, UID tags are free тАФ respawn and verify.
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
    UE_LOG(LogTemp, Warning, TEXT("DEVMODE_SWEEP_RESULT %s failures=%d"),
        SweepFailures == 0 ? TEXT("PASS") : TEXT("FAIL"), SweepFailures);
}

// ============================================================================
// devmode.expect <Key> <Op> <Value>  (Keys: hp|mobs|inv|gold; Ops: eq|ne|gt|lt|ge|le)
// ============================================================================
void UDevModeConsoleCommands::Cmd_Expect(const TArray<FString>& Args) const
{
    if (Args.Num() < 3)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.expect <hp|mobs|inv|gold> <eq|ne|gt|lt|ge|le> <Value>"));
        return;
    }
    ExpectValue(Args[0].ToLower(), Args[1].ToLower(), FCString::Atoi(*Args[2]));
}

void UDevModeConsoleCommands::ExpectValue(const FString& Key, const FString& Op, int32 Expected) const
{
    int32 Actual = -1;
    UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;

    if (Key == TEXT("hp"))
    {
        if (GameInstance && GameInstance->Player)
        {
            Actual = GameInstance->Player->GetPlayerCurrentHPPoints();
        }
    }
    else if (Key == TEXT("mobs"))
    {
        if (World)
        {
            Actual = 0;
            for (TActorIterator<ABasicMOB> It(World); It; ++It)
            {
                if (!(*It)->GetMOBIsDead())
                {
                    ++Actual;
                }
            }
        }
    }
    else if (Key == TEXT("inv"))
    {
        if (GameInstance && GameInstance->InventoryManager)
        {
            Actual = GameInstance->InventoryManager->GetInventory().items.Num();
        }
    }
    else if (Key == TEXT("gold"))
    {
        if (GameInstance && GameInstance->InventoryManager)
        {
            Actual = GameInstance->InventoryManager->GetInventory().gold;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_ASSERT FAIL unknown-key '%s'"), *Key);
        ++SweepFailures;
        return;
    }

    bool bPass = false;
    if (Op == TEXT("eq")) bPass = (Actual == Expected);
    else if (Op == TEXT("ne")) bPass = (Actual != Expected);
    else if (Op == TEXT("gt")) bPass = (Actual > Expected);
    else if (Op == TEXT("lt")) bPass = (Actual < Expected);
    else if (Op == TEXT("ge")) bPass = (Actual >= Expected);
    else if (Op == TEXT("le")) bPass = (Actual <= Expected);
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_ASSERT FAIL unknown-op '%s'"), *Op);
        ++SweepFailures;
        return;
    }

    if (bPass)
    {
        UE_LOG(LogTemp, Log, TEXT("DEVMODE_ASSERT PASS %s %s %d actual=%d"), *Key, *Op, Expected, Actual);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DEVMODE_ASSERT FAIL %s %s %d actual=%d"), *Key, *Op, Expected, Actual);
        ++SweepFailures;
    }
}

// ============================================================================
// devmode.setmobhp <MobUID> <Value>  (<=0 also runs Die())
// ============================================================================
void UDevModeConsoleCommands::Cmd_SetMobHP(const TArray<FString>& Args) const
{
    if (Args.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.setmobhp <MobUID> <Value>"));
        return;
    }
    if (!GameInstance || !GameInstance->MOBManager) return;

    AActor* MobActor = GameInstance->MOBManager->FindMobActor(FCString::Atoi(*Args[0]));
    ABasicMOB* Mob = Cast<ABasicMOB>(MobActor);
    if (!Mob)
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.setmobhp: Mob UID=%s not found"), *Args[0]);
        return;
    }

    const int32 NewHP = FCString::Atoi(*Args[1]);
    Mob->SetMOBCurrentHealth(NewHP);
    if (NewHP <= 0)
    {
        Mob->Die();
    }
    UE_LOG(LogTemp, Log, TEXT("devmode.setmobhp: Mob UID=%s HP set to %d"), *Args[0], NewHP);
}

// ============================================================================
// devmode.killmob [MobUID]  (first alive mob when omitted)
// ============================================================================
void UDevModeConsoleCommands::Cmd_KillMob(const TArray<FString>& Args) const
{
    if (!GameInstance || !GameInstance->MOBManager) return;

    ABasicMOB* Target = nullptr;
    if (Args.Num() >= 1)
    {
        Target = Cast<ABasicMOB>(GameInstance->MOBManager->FindMobActor(FCString::Atoi(*Args[0])));
    }
    else
    {
        UWorld* World = GameInstance->GetWorld();
        if (World)
        {
            for (TActorIterator<ABasicMOB> It(World); It; ++It)
            {
                if (!(*It)->GetMOBIsDead())
                {
                    Target = *It;
                    break;
                }
            }
        }
    }

    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.killmob: no living mob found"));
        return;
    }
    Target->Die();
    UE_LOG(LogTemp, Log, TEXT("devmode.killmob: mob '%s' killed"), *Target->GetMobName());
}

// ============================================================================
// devmode.clearcorpses тАФ destroy dead mob actors so their UID tags free up.
// ============================================================================
void UDevModeConsoleCommands::Cmd_ClearCorpses(const TArray<FString>& Args) const
{
    UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode.clearcorpses: No world"));
        return;
    }
    int32 Removed = 0;
    TArray<AActor*> ToDestroy;
    for (TActorIterator<ABasicMOB> It(World); It; ++It)
    {
        if ((*It)->GetMOBIsDead())
        {
            ToDestroy.Add(*It);
        }
    }
    for (AActor* A : ToDestroy)
    {
        A->Destroy();
        ++Removed;
    }
    UE_LOG(LogTemp, Log, TEXT("devmode.clearcorpses: destroyed %d corpses"), Removed);
}

void UDevModeConsoleCommands::SweepShot(const FString& Tag) const
{
    // Screenshots only make sense with a renderer; nightly nullrhi runs skip them.
    if (!FParse::Param(FCommandLine::Get(), TEXT("devmode-shots")))
    {
        return;
    }
    FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("DevSweep_%s"), *Tag), true, true);
    UE_LOG(LogTemp, Log, TEXT("DEVMODE_SWEEP_SHOT %s"), *Tag);
}
