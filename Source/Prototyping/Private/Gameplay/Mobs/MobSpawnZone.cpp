// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MobSpawnZone.h"
#include <Gameplay/Players/BasicPlayer.h>

// Sets default values
AMobSpawnZone::AMobSpawnZone()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


}

// Called when the game starts or when spawned
void AMobSpawnZone::BeginPlay()
{
	Super::BeginPlay();
	UBoxComponent* BoxComponent = Cast<UBoxComponent>(GetCollisionComponent());

	// Bind the overlap events
	if (BoxComponent)
	{
		BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AMobSpawnZone::OnOverlapBegin);
		BoxComponent->OnComponentEndOverlap.AddDynamic(this, &AMobSpawnZone::OnOverlapEnd);
	}
}

// Called every frame
void AMobSpawnZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// change spawn zone CollisionComponent box component size based on the start and end positions
void AMobSpawnZone::ChangeSpawnZoneSize()
{
	UBoxComponent* BoxComponent = Cast<UBoxComponent>(GetCollisionComponent());
	if (!BoxComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("BoxComponent not found"));
		return;
	}

	// Берём Pos и Size из SpawnZoneData
	FVector Pos = SpawnZoneData.spawnStartPos;
	FVector Size = SpawnZoneData.spawnSize;

	//set z position to half of the size + the start position
	Pos.Z += (Size.Z / 2);

	BoxComponent->SetBoxExtent(Size / 2);
	BoxComponent->SetWorldLocation(Pos);

	// Обновляем Bounds
	BoxComponent->UpdateBounds();
}


// set the spawn zone data
void AMobSpawnZone::SetSpawnZoneData(FSpawnZoneStruct Data)
{
	if (SpawnZoneData.zoneID == 0)
	{
		SpawnZoneData = Data;

		// change the spawn zone size
		ChangeSpawnZoneSize();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Spawn Zone Data already set"));
	}
}

void AMobSpawnZone::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && (OtherActor != this))
	{
		// Handle overlap start logic here
		UE_LOG(LogTemp, Warning, TEXT("Overlap Begin with %s"), *OtherActor->GetName());

		// check if the actor is a player
		ABasicPlayer* Player = Cast<ABasicPlayer>(OtherActor);

		if (Player)
		{
			// check if the spawn zone is enabled
			if (SpawnZoneData.bSpawningEnabled)
			{
				Player->SetCurrentZoneName(SpawnZoneData.zoneName);
				Player->ZoneUpdated.Broadcast();
			}
		}
	}
}

void AMobSpawnZone::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && (OtherActor != this))
	{
		// Handle overlap end logic here
		UE_LOG(LogTemp, Warning, TEXT("Overlap End with %s"), *OtherActor->GetName());

		// check if the actor is a player
		ABasicPlayer* Player = Cast<ABasicPlayer>(OtherActor);

		if (Player)
		{
			Player->SetCurrentZoneName("");
			Player->ZoneUpdated.Broadcast();
		}
	}
}

// get the spawn zone data
FSpawnZoneStruct AMobSpawnZone::GetSpawnZoneData()
{
	return SpawnZoneData;
}

void AMobSpawnZone::SetSpawnZoneTag(FName spawnZoneTag)
{
	Tags.Add(spawnZoneTag);
}

// set the spawn zone status
void AMobSpawnZone::SetSpawnZoneStatus(bool spawnZoneStatus)
{
	SpawnZoneData.bSpawningEnabled = spawnZoneStatus;
}

// get the spawn zone status
bool AMobSpawnZone::GetSpawnZoneStatus()
{
	return SpawnZoneData.bSpawningEnabled;
}

// set the spawn zone ID
void AMobSpawnZone::SetSpawnZoneID(int spawnZoneID)
{
	SpawnZoneData.zoneID = spawnZoneID;
}

// get the spawn zone ID
int AMobSpawnZone::GetSpawnZoneID()
{
	return SpawnZoneData.zoneID;
}

// set the spawn zone name
void AMobSpawnZone::SetSpawnZoneName(FString spawnZoneName)
{
	SpawnZoneData.zoneName = spawnZoneName;
}

// get the spawn zone name
FString AMobSpawnZone::GetSpawnZoneName()
{
	return SpawnZoneData.zoneName;
}

// set the spawn zone position start
void AMobSpawnZone::SetSpawnZonePositionStart(FVector spawnZonePosition)
{
	SpawnZoneData.spawnStartPos = spawnZonePosition;
}

// get the spawn zone position start
FVector AMobSpawnZone::GetSpawnZonePositionStart()
{
	return SpawnZoneData.spawnStartPos;
}

// set the spawn zone position end
void AMobSpawnZone::SetSpawnZonePositionEnd(FVector spawnZonePosition)
{
	SpawnZoneData.spawnSize = spawnZonePosition;
}

// get the spawn zone position end
FVector AMobSpawnZone::GetSpawnZonePositionEnd()
{
	return SpawnZoneData.spawnSize;
}

// set the spawn zone mob ID
void AMobSpawnZone::SetSpawnZoneMobID(int spawnZoneMobID)
{
	SpawnZoneData.MobIDToSpawn = spawnZoneMobID;
}

// get the spawn zone mob ID
int AMobSpawnZone::GetSpawnZoneMobID()
{
	return SpawnZoneData.MobIDToSpawn;
}

// set the spawn zone max mobs count
void AMobSpawnZone::SetSpawnZoneMaxMobsCount(int spawnZoneMaxMobsCount)
{
	SpawnZoneData.MaxMobs = spawnZoneMaxMobsCount;
}

// get the spawn zone max mobs count
int AMobSpawnZone::GetSpawnZoneMaxMobsCount()
{
	return SpawnZoneData.MaxMobs;
}

// set the spawn zone current mobs count
void AMobSpawnZone::SetSpawnZoneCurrentMobsCount(int spawnZoneCurrentMobsCount)
{
	SpawnZoneData.currentMobsCount = spawnZoneCurrentMobsCount;
}

// get the spawn zone current mobs count
int AMobSpawnZone::GetSpawnZoneCurrentMobsCount()
{
	return SpawnZoneData.currentMobsCount;
}

// set the spawn zone respawn time
void AMobSpawnZone::SetSpwanZoneRespawnTime(int spawnZoneRespawnTime)
{
	SpawnZoneData.respawnTime = spawnZoneRespawnTime;
}

// get the spawn zone respawn time
int AMobSpawnZone::GetSpawnZoneRespawnTime()
{
	return SpawnZoneData.respawnTime;
}

// get spawn zone spawned mobs
TArray<FString> AMobSpawnZone::GetSpawnedMobs()
{
	return SpawnedMobsUID;
}

// set spawn zone spawned mobs
void AMobSpawnZone::SetSpawnedMobs(TArray<FString> Mobs)
{
	SpawnedMobsUID = Mobs;
}