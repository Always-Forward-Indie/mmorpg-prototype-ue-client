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

    // We need Config — get it from GameInstance
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
