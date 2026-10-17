// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "CrashDiagnostics.h"

void UMOBManager::SpawnMOB(const FMOBStruct& MOBData)
{
	if (!worldContext || !worldContext->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBManager: No valid world context — queuing MOB %s for later spawn"), *MOBData.mobUniqueID);
		PendingMOBSpawns.Add(MOBData);
		return;
	}

	// If MOB exists in the world do not spawn it
	if (MOBExists(worldContext, FName(MOBData.mobUniqueID)))
	{
		UE_LOG(LogTemp, Warning, TEXT("MOB already exists in the world"));
		return;
	}

	// Skip spawn if this corpse was already removed by the server
	const int32 MobUID = FCString::Atoi(*MOBData.mobUniqueID);
	if (MobUID > 0 && RemovedCorpseUIDs.Contains(MobUID))
	{
		UE_LOG(LogTemp, Log, TEXT("MOBManager: Skipping spawn for UID %d — corpse already removed"), MobUID);
		return;
	}


	// get the game instance
	if (gameInstance)
	{
		// get basic MOB class
		TSubclassOf<class ABasicMOB> BasicMOBClass = gameInstance->GetBasicMOBClass();

		if (BasicMOBClass == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Basic MOB class not found"));
			return;
		}

		// spawn the MOB
		ABasicMOB* MOB = worldContext->SpawnActor<ABasicMOB>(
			BasicMOBClass,
			FVector(MOBData.mobPosition.positionX, 
				MOBData.mobPosition.positionY, 
				MOBData.mobPosition.positionZ), 
			FRotator(0, 0, 0)
		);

		if (MOB == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("MOB not spawned"));
			return;
		}

		MOB->SetupMobVisual(FName(MOBData.mobSlug));
		MOB->SetupMobAudio(FName(MOBData.mobSlug));

		// set the MOB data
		MOB->SetMOBData(MOBData);
		// set the MOB tag
		MOB->SetMOBTag(MOBData.mobUniqueID);
		MOB->SetMOBTag("Mob");

		// Always seed the movement component so bHasReceivedPacket is true
		// before the first mobMoveUpdate arrives.  Without this, the first
		// packet hits the "first packet ever" branch and hard-teleports the
		// mob instead of smoothly interpolating.
		if (MOB->MOBMovementComponent)
		{
			const FVector VelDir(MOBData.mobVelocity.dirX, MOBData.mobVelocity.dirY, 0.f);
			const FVector SpawnPos(MOBData.mobPosition.positionX,
								   MOBData.mobPosition.positionY,
								   MOB->GetActorLocation().Z);
			MOB->MOBMovementComponent->InitializeFromSpawnData(
				SpawnPos, VelDir, MOBData.mobVelocity.speed, MOBData.mobCombatState);

			MOB->MOBMovementComponent->SnapToGround();
		}
	}
}

// Check if a MOB exists in the world
bool UMOBManager::MOBExists(UWorld* World, const FName& Tag)
{
	if (!World || !World->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("MOBManager::MOBExists: Invalid world context"));
		return false;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(World, Tag, FoundActors);

	// If the array is not empty, then actors with the tag exist
	return FoundActors.Num() > 0;
}

AActor* UMOBManager::FindMobActor(int32 MobUID) const
{
	const TWeakObjectPtr<ABasicMOB>* Found = MobActorRegistry.Find(MobUID);
	if (Found && Found->IsValid())
	{
		return Found->Get();
	}
	// Fallback: tag-based search
	if (!worldContext) return nullptr;
	const FName UIDTag = FName(*FString::FromInt(MobUID));
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(worldContext, UIDTag, FoundActors);
	return FoundActors.Num() > 0 ? FoundActors[0] : nullptr;
}

void UMOBManager::RegisterMob(int32 MobUID, ABasicMOB* MobActor)
{
	if (MobUID > 0 && IsValid(MobActor))
	{
		MobActorRegistry.Add(MobUID, MobActor);
	}
}

void UMOBManager::UnregisterMob(int32 MobUID)
{
	MobActorRegistry.Remove(MobUID);
}

void UMOBManager::RegisterPlayer(int32 PlayerId, ABasicPlayer* PlayerActor)
{
	if (PlayerId > 0 && IsValid(PlayerActor))
	{
		PlayerRegistry.Add(PlayerId, PlayerActor);
	}
}

void UMOBManager::UnregisterPlayer(int32 PlayerId)
{
	PlayerRegistry.Remove(PlayerId);
}

void UMOBManager::ClearWorldState()
{
	MobActorRegistry.Empty();
	PlayerRegistry.Empty();
}

void UMOBManager::FlushPendingSpawns()
{
	if (PendingMOBSpawns.Num() == 0) return;

	UE_LOG(LogTemp, Warning, TEXT("MOBManager: Flushing %d pending MOB spawns"), PendingMOBSpawns.Num());
	TArray<FMOBStruct> Spawns = MoveTemp(PendingMOBSpawns);
	PendingMOBSpawns.Empty();

	for (const FMOBStruct& MobData : Spawns)
	{
		SpawnMOB(MobData);
	}
}
