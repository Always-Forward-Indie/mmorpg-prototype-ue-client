// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "Data/DataStructs.h"
#include "Components/BoxComponent.h"
#include "MobSpawnZone.generated.h"

/**
 * 
 */
UCLASS()
class PROTOTYPING_API AMobSpawnZone : public ATriggerBox
{
	GENERATED_BODY()

	private:
		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpawnZone Data", meta = (AllowPrivateAccess = "true"))
		FSpawnZoneStruct SpawnZoneData;

		UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpawnZone Data", meta = (AllowPrivateAccess = "true"))
		TArray<FString> SpawnedMobsUID;
	
	public:
		AMobSpawnZone();

		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZoneData(FSpawnZoneStruct Data);

		// Declare the overlap start event handler
		UFUNCTION()
		void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

		// Declare the overlap end event handler
		UFUNCTION()
		void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZoneTag(FName spawnZoneTag);
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZoneStatus(bool spawnZoneStatus);
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZoneID(int spawnZoneID);
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZoneName(FString spawnZoneName);
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZonePositionStart(FVector spawnZonePosition);
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZonePositionEnd(FVector spawnZonePosition);

		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZoneMobID(int spawnZoneMobID);
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZoneMaxMobsCount(int spawnZoneMaxMobsCount);
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnZoneCurrentMobsCount(int spawnZoneCurrentMobsCount);
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpwanZoneRespawnTime(int spawnZoneRespawnTime);

		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		FSpawnZoneStruct GetSpawnZoneData();

		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		bool GetSpawnZoneStatus();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		int GetSpawnZoneID();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		FString GetSpawnZoneName();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		FVector GetSpawnZonePositionStart();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		FVector GetSpawnZonePositionEnd();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		int GetSpawnZoneMobID();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		int GetSpawnZoneMaxMobsCount();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		int GetSpawnZoneCurrentMobsCount();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		int GetSpawnZoneRespawnTime();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		TArray<FString> GetSpawnedMobs();
		UFUNCTION(BlueprintCallable, Category = "Spawn Zone")
		void SetSpawnedMobs(TArray<FString> Mobs);

	protected:
		// Called when the game starts or when spawned
		virtual void BeginPlay() override;

	public:
		// Called every frame
		virtual void Tick(float DeltaTime) override;

		void ChangeSpawnZoneSize();

};
