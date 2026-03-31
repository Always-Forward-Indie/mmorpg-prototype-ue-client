// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Utils/JSONParser.h"
#include "Networking/PingManager.h"
#include "Networking/NetworkManager.h"
#include <Kismet/GameplayStatics.h>

#include "PlayerManager.generated.h"

// Forward declarations
class UMyGameInstance;
class UCombatSystemManager;
class UCombatNetworkHandler;

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UPlayerManager : public UObject
{
	GENERATED_BODY()

private:
UWorld* worldContext = nullptr;

UPROPERTY()
UPingManager* pingManager = nullptr;

UPROPERTY()
UNetworkManager* networkManager = nullptr;

UPROPERTY()
UMyGameInstance* gameInstance = nullptr;

// Combat network handler for delegating combat events
UPROPERTY()
UCombatNetworkHandler* CombatNetworkHandler;

public:
	UPlayerManager(const FObjectInitializer& ObjectInitializer);
	void Initialize(UNetworkManager* NetworkManager, UPingManager* PingManager);
	void SubscribeToNetworkManager();
	void SetWorldContext(UWorld* World);

	UFUNCTION()
	void ProcessGameServerData(const FString& ReceivedData);
	UFUNCTION()
	void ProcessChunkServerData(const FString& ReceivedData);
	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendJoinGameRequest(const FClientDataStruct& ClientData);
	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendJoinCharacterChunkRequest(const FClientDataStruct& ClientData);

	// Phase 1: Register client session on Chunk Server (joinGameClient)
	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendJoinClientChunkRequest(const FClientDataStruct& ClientData);

	// Phase 3: Send playerReady ACK after game world is fully loaded
	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendPlayerReadyRequest(const FClientDataStruct& ClientData);

	void SendGetConnectedPlayersRequest(FClientDataStruct& ClientData);
	void SendMovePlayerRequest(FClientDataStruct& ClientData);
	void SendLeaveGameRequest(FClientDataStruct& ClientData);

	// Start TimeSyncService-based ping updates
	void StartPing();
	
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SendPlayerAttackRequest(const FClientDataStruct& ClientData, int32 TargetID, const FString& SkillSlug, int32 TargetTypeId);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendRespawnRequest(const FClientDataStruct& ClientData);

private:
	// Initialize combat network handler
	void InitializeCombatNetworkHandler();
	
	// Check if event is combat-related
	bool IsCombatEvent(const FString& EventType) const;
};
