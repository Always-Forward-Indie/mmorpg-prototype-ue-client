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
//   devmode.reloadmobs                 - destroy all dev mobs and re-spawn from JSON
//   devmode.reloadinventory            - clear and re-populate inventory from JSON
//   devmode.listmobs                   - print all spawned mobs to log
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
    void Cmd_SetPlayerHP(const TArray<FString>& Args) const;
    void Cmd_ReloadMobs(const TArray<FString>& Args) const;
    void Cmd_ReloadInventory(const TArray<FString>& Args) const;
    void Cmd_ListMobs(const TArray<FString>& Args) const;

    // Command names - kept to allow clean unregistration by name
    TArray<FString> RegisteredCommandNames;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;
};
