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
