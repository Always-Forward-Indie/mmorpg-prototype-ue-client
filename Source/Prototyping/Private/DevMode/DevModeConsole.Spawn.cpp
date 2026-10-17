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
    SnapPlayerToGround();
}

void UDevModeConsoleCommands::SnapPlayerToGround() const
{
    if (!GameInstance || !GameInstance->Player) return;
    UWorld* World = GameInstance->GetWorld();
    ABasicPlayer* Player = GameInstance->Player;
    if (!World || !Player) return;

    const FVector From = Player->GetActorLocation() + FVector(0.0f, 0.0f, 2000.0f);
    const FVector To = Player->GetActorLocation() - FVector(0.0f, 0.0f, 5000.0f);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DevModeSnapToGround), false, Player);
    if (World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Params) && Hit.bBlockingHit)
    {
        float HalfHeight = 90.0f;
        if (const UCapsuleComponent* Capsule = Player->GetCapsuleComponent())
        {
            HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        }
        Player->SetActorLocation(Hit.Location + FVector(0.0f, 0.0f, HalfHeight + 2.0f));
        UE_LOG(LogTemp, Log, TEXT("devmode: player snapped to ground Z=%.0f"), Hit.Location.Z);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("devmode: no ground under player, keeping position"));
    }
}

// ============================================================================
// devmode.setplayerhp <Value>
// ============================================================================
