// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Utils/JSONParser.h"
#include "Networking/NetworkManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include <Kismet/GameplayStatics.h>

#include "MOBManager.generated.h"

class UMyGameInstance;

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UMOBManager : public UObject
{
	GENERATED_BODY()

	private:
		UWorld* worldContext = nullptr;
		UNetworkManager* networkManager;
		UMyGameInstance* gameInstance;

		// Check if event is combat-related
		bool IsCombatEvent(const FString& EventType) const;

	public:
		UMOBManager(const FObjectInitializer& ObjectInitializer);
		void Initialize(UNetworkManager* NetworkManager);
		void SubscribeToNetworkManager();
		void SetWorldContext(UWorld* World);
		UFUNCTION()
		void ProcessGameServerData(const FString& ReceivedData);
		void SendGetMobData(const FClientDataStruct& ClientData);
		void SetGameInstance(UMyGameInstance* GameInstance);

		// Combat related methods
		void UpdateMobHealth(const FCombatResultData& ResultData);

		UFUNCTION(BlueprintCallable, Category = "MOBManager")
		void SpawnMOB(const FMOBStruct& MOBData);
		UFUNCTION(BlueprintCallable, Category = "MOBManager")
		bool MOBExists(UWorld* World, const FName& Tag);

		/** Find the spawned ABasicMOB actor for a given mob unique ID. Returns nullptr if not found. */
		UFUNCTION(BlueprintCallable, Category = "MOBManager")
		AActor* FindMobActor(int32 MobUID) const;

		/** Register a spawned mob actor in the UID-to-actor map. */
		void RegisterMob(int32 MobUID, class ABasicMOB* MobActor);

		/** Unregister a mob actor from the UID-to-actor map. */
		void UnregisterMob(int32 MobUID);

		/** Register a player actor so mobs can find their aggro targets. */
		void RegisterPlayer(int32 PlayerId, class ABasicPlayer* PlayerActor);

		/** Unregister a player actor. */
		void UnregisterPlayer(int32 PlayerId);

	private:
		/** Fast UID ? actor lookup map populated by RegisterMob. */
		TMap<int32, TWeakObjectPtr<class ABasicMOB>> MobActorRegistry;

		/** Player registry for target resolution. */
		TMap<int32, TWeakObjectPtr<class ABasicPlayer>> PlayerRegistry;
};
