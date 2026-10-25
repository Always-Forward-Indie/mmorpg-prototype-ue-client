#include "DevMode/DevModeConsoleCommands.h"
#include "Prototyping.h"
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

void UDevModeConsoleCommands::Cmd_Expect(const TArray<FString>& Args) const
{
    if (Args.Num() < 3)
    {
        UE_LOG(LogGameplay, Warning, TEXT("Usage: devmode.expect <hp|mobs|inv|gold> <eq|ne|gt|lt|ge|le> <Value>"));
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
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_ASSERT FAIL unknown-key '%s'"), *Key);
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
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_ASSERT FAIL unknown-op '%s'"), *Op);
        ++SweepFailures;
        return;
    }

    if (bPass)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("DEVMODE_ASSERT PASS %s %s %d actual=%d"), *Key, *Op, Expected, Actual);
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("DEVMODE_ASSERT FAIL %s %s %d actual=%d"), *Key, *Op, Expected, Actual);
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
        UE_LOG(LogGameplay, Warning, TEXT("Usage: devmode.setmobhp <MobUID> <Value>"));
        return;
    }
    if (!GameInstance || !GameInstance->MOBManager) return;

    AActor* MobActor = GameInstance->MOBManager->FindMobActor(FCString::Atoi(*Args[0]));
    ABasicMOB* Mob = Cast<ABasicMOB>(MobActor);
    if (!Mob)
    {
        UE_LOG(LogGameplay, Warning, TEXT("devmode.setmobhp: Mob UID=%s not found"), *Args[0]);
        return;
    }

    const int32 NewHP = FCString::Atoi(*Args[1]);
    Mob->SetMOBCurrentHealth(NewHP);
    if (NewHP <= 0)
    {
        Mob->Die();
    }
    UE_LOG(LogGameplay, Verbose, TEXT("devmode.setmobhp: Mob UID=%s HP set to %d"), *Args[0], NewHP);
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
        UE_LOG(LogGameplay, Warning, TEXT("devmode.killmob: no living mob found"));
        return;
    }
    Target->Die();
    UE_LOG(LogGameplay, Verbose, TEXT("devmode.killmob: mob '%s' killed"), *Target->GetMobName());
}

// ============================================================================
// devmode.clearcorpses — destroy dead mob actors so their UID tags free up.
// ============================================================================
void UDevModeConsoleCommands::Cmd_ClearCorpses(const TArray<FString>& Args) const
{
    UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
    if (!World)
    {
        UE_LOG(LogGameplay, Warning, TEXT("devmode.clearcorpses: No world"));
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
    UE_LOG(LogGameplay, Verbose, TEXT("devmode.clearcorpses: destroyed %d corpses"), Removed);
}

