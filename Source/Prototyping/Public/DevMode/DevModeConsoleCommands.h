#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DevModeConsoleCommands.generated.h"

class UMyGameInstance;

// ============================================================================
// DevModeConsoleCommands
// Registers console commands that are available when DevMode is active.
// All commands are unregistered when this object is destroyed.
//
// Available commands:
//   devmode.spawnmob <MobID>           - spawn a mob from dev_mobs.json by mobID
//   devmode.setcombatstate <MobUID> <State> - push a combat state to a mob's movement component
//   devmode.teleport <X> <Y> <Z>       - teleport the local player
//   devmode.setplayerhp <Value>        - set local player current HP
//   devmode.setmobhp <MobUID> <Value>  - set mob HP (<=0 also runs Die())
//   devmode.killmob [MobUID]           - run Die() on one mob (first alive if no arg)
//   devmode.expect <Key> <Op> <Value>  - assert live state; Keys: hp|mobs|inv|gold; Ops: eq|ne|gt|lt|ge|le
//   devmode.reloadmobs                 - destroy all dev mobs and re-spawn from JSON
//   devmode.reloadinventory            - clear and re-populate inventory from JSON
//   devmode.listmobs                   - print all spawned mobs to log
//   devmode.scenario <combat|reset|full_sweep|bot_combat_patrol|bot_loot_pickup|bot_death_respawn|bot_harvest_run|bot_periphery> - one-shot QA flows (teleport/reload presets)
//   devmode.bot <combat_patrol|loot_pickup|death_respawn|harvest_run|periphery>      - start the L1 action bot (real movement/combat, see DevModeBotBrain)
// Assert markers for external log scan: "DEVMODE_ASSERT <PASS|FAIL> ...",
// sweep tombstones: "DEVMODE_SWEEP_BEGIN <name>", "DEVMODE_SWEEP_RESULT <PASS|FAIL> failures=N",
// bot tombstones: "DEVMODE_BOT_BEGIN <name>", "DEVMODE_BOT_RESULT <name> <PASS|FAIL> ...".
// ============================================================================
UCLASS()
class PROTOTYPING_API UDevModeConsoleCommands : public UObject
{
    GENERATED_BODY()

public:
// Register all devmode.* console commands.
void RegisterCommands(UMyGameInstance* InGameInstance);

// Remove all registered commands (called from cleanup or GC).
void UnregisterCommands();

// Called by GC before the object is destroyed - unregister commands
// so we never hold dangling IConsoleObject* after the UObject is gone.
virtual void BeginDestroy() override;

private:
    // --- Command implementations ---
    void Cmd_SpawnMob(const TArray<FString>& Args) const;
    void Cmd_SetCombatState(const TArray<FString>& Args) const;
    void Cmd_Teleport(const TArray<FString>& Args) const;
    // Drop the player straight down onto walkable ground (handy after teleports
    // to hand-authored coordinates that may hover or sit inside geometry).
    void SnapPlayerToGround() const;
    void Cmd_SetPlayerHP(const TArray<FString>& Args) const;
    void Cmd_ReloadMobs(const TArray<FString>& Args) const;
    void Cmd_ReloadInventory(const TArray<FString>& Args) const;
    void Cmd_ListMobs(const TArray<FString>& Args) const;
    void Cmd_Scenario(const TArray<FString>& Args) const;
    void Cmd_Expect(const TArray<FString>& Args) const;
    void Cmd_SetMobHP(const TArray<FString>& Args) const;
    void Cmd_KillMob(const TArray<FString>& Args) const;
    void Cmd_ClearCorpses(const TArray<FString>& Args) const;

    // Screenshot helper for visual sweep runs (-devmode-shots flag).
    void SweepShot(const FString& Tag) const;

    // L1 action bot (DevModeBotBrain, owned by the GameInstance member so GC
    // cannot collect it mid-run тАФ LoadMap force-collects unreferenced objects).
    void StartBotBrain(const FString& ScenarioName = TEXT("combat_patrol")) const;

    // Shared assert core (also used by full_sweep). Logs DEVMODE_ASSERT PASS/FAIL.
    void ExpectValue(const FString& Key, const FString& Op, int32 Expected) const;

    // Sweep failure counter for the DEVMODE_SWEEP_RESULT tombstone.
    mutable int32 SweepFailures = 0;

    // Command names - kept to allow clean unregistration by name
    TArray<FString> RegisteredCommandNames;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;
};
