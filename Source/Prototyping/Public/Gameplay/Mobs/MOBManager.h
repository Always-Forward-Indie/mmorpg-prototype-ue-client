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

	public:
		UMOBManager(const FObjectInitializer& ObjectInitializer);
		void Initialize(UNetworkManager* NetworkManager);
		void SubscribeToNetworkManager();
		void SetWorldContext(UWorld* World);
		UFUNCTION()
		void ProcessGameServerData(const FString& ReceivedData);
		void SendGetMobData(const FClientDataStruct& ClientData);
		void SetGameInstance(UMyGameInstance* GameInstance);

		UFUNCTION(BlueprintCallable, Category = "MOBManager")
		void SpawnMOB(const FMOBStruct& MOBData);
		UFUNCTION(BlueprintCallable, Category = "MOBManager")
		bool MOBExists(UWorld* World, const FName& Tag);
};
