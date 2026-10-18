#include "DevMode/DevModeConsoleCommands.h"
#include "DevMode/DevModeBotBrain.h"
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
#include "HighResScreenshot.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
        TEXT("DevMode: one-shot QA flows. Usage: devmode.scenario <combat|reset|full_sweep|soak|bot_combat_patrol>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_Scenario(A); }
        }));

    Reg(TEXT("devmode.bot"),
        TEXT("DevMode: start the L1 action bot. Usage: devmode.bot <combat_patrol|loot_pickup|death_respawn|harvest_run|periphery>"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get())
            {
                if (A.Num() < 1
                    || (!A[0].Equals(TEXT("combat_patrol"), ESearchCase::IgnoreCase)
                        && !A[0].Equals(TEXT("loot_pickup"), ESearchCase::IgnoreCase)
                        && !A[0].Equals(TEXT("death_respawn"), ESearchCase::IgnoreCase)
                        && !A[0].Equals(TEXT("harvest_run"), ESearchCase::IgnoreCase)
                        && !A[0].Equals(TEXT("periphery"), ESearchCase::IgnoreCase)))
                {
                    UE_LOG(LogTemp, Warning, TEXT("Usage: devmode.bot <combat_patrol|loot_pickup|death_respawn|harvest_run|periphery>"));
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

    Reg(TEXT("devmode.dropsocket"),
        TEXT("DevMode: simulate a mid-session socket drop. Usage: devmode.dropsocket chunk"),
        FConsoleCommandWithArgsDelegate::CreateLambda([WeakThis](const TArray<FString>& A)
        {
            if (UDevModeConsoleCommands* Cmd = WeakThis.Get()) { Cmd->Cmd_DropSocket(A); }
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

