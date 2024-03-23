// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Utils/JSONParser.h"
#include "Networking/NetworkManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MobSpawnZone.h"
#include <Kismet/GameplayStatics.h>

#include "SpawnZoneManager.generated.h"

class UMyGameInstance;

/**
 * 
 */
UCLASS()
class PROTOTYPING_API USpawnZoneManager : public UObject
{
	GENERATED_BODY()

	private:
		UWorld* worldContext = nullptr;
		UNetworkManager* networkManager;
		UMyGameInstance* gameInstance;
		//list of spawn zones
		TArray<AMobSpawnZone*> SpawnZones;

	public:
		USpawnZoneManager(const FObjectInitializer& ObjectInitializer);
		void Initialize(UNetworkManager* NetworkManager);
		void SetWorldContext(UWorld* World);
		void SubscribeToNetworkManager();
		UFUNCTION()
		void ProcessGameServerData(const FString& ReceivedData);
		void SendGetSpawnZonesData(const FClientDataStruct& ClientData);
		void SetGameInstance(UMyGameInstance* GameInstance);
		UFUNCTION(BlueprintCallable, Category = "SpawnZoneManager")
		void CreateSpawnZone(const FSpawnZoneStruct& SpawnZoneData);
		UFUNCTION(BlueprintCallable, Category = "SpawnZoneManager")
		bool SpawnZoneExists(UWorld* World, const FName& Tag);
};
