// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Utils/JSONParser.h"
#include "Networking/NetworkManager.h"
#include "Networking/PingManager.h"
#include <Kismet/GameplayStatics.h>

#include "AuthenticationManager.generated.h"

class UMyGameInstance;

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UAuthenticationManager : public UObject
{
	GENERATED_BODY()
private:
UWorld* worldContext = nullptr;
// ping manager
UPROPERTY()
UPingManager* pingManager = nullptr;

UPROPERTY()
UNetworkManager* networkManager = nullptr;

UPROPERTY()
UMyGameInstance* gameInstance = nullptr;

public:
	UAuthenticationManager(const FObjectInitializer& ObjectInitializer);
	void Initialize(UNetworkManager* NetworkManager, UPingManager* PingManager);
	void SubscribeToNetworkManager();
	void SetWorldContext(UWorld* World);
	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendLoginRequest(const FString& Username, const FString& Password);
	void SendCharacterListRequest(FClientDataStruct& ClientData);
	void SendLeaveGameRequest(FClientDataStruct& ClientData);
	UFUNCTION()
	void ProcessLoginResponse(const FString& ReceivedData);

	bool IsLoginValueValid(const FString& Login);
	bool IsPasswordValueValid(const FString& Password);

	// Start TimeSyncService-based ping updates
	void StartPing();
};
