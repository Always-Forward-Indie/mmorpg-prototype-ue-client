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
	UPingManager* pingManager;
	UNetworkManager* networkManager;
	UMyGameInstance* gameInstance;

	// Timer to ping servers
	FTimerHandle NetworkServersPingTimerHandle;
	TMap<FString, TArray<FDateTime>> ReceiveTimes; // Array to store ReceiveTime values for ping calculation
	TMap<FString, TArray<FDateTime>> SendTimes; // Array to store SendTime values for ping calculation
	// count of packets for ping calculation
	int32 PingPacketsCount = 12;
	float PingTimeout = 2.0f; // Ping timeout in seconds

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

	void SendPingRequest();
	void StartPing();
	void AddReceivePingTime(const FString& EventType, const FDateTime& ReceiveTime);
	void AddSendPingTime(const FString& EventType, const FDateTime& SendTime);
};
