// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "Networking/NetworkReceiverWorker.h"
#include "Networking/NetworkSenderWorker.h"
#include "NetworkManager.generated.h"


// Define a delegate or event signature
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoginServerDataReceived, const FString&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameServerDataReceived, const FString&, Data);

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UNetworkManager : public UObject
{
	GENERATED_BODY()
private:
	UWorld* WorldContext = nullptr;

	// Login server details
	FString LoginServerIP;
	int32 LoginServerPort;
	FSocket* LoginServerSocket;
	bool bIsLoginSocketConnected;

	NetworkReceiverWorker* ReceiverLoginServerWorker;
	FRunnableThread* ReceiverLoginServerThread;

	NetworkSenderWorker* SenderLoginServerWorker;
	FRunnableThread* SenderLoginServerThread;

	// Game server details
	FString GameServerIP;
	int32 GameServerPort;
	FSocket* GameServerSocket;
	bool bIsGameSocketConnected;

	NetworkReceiverWorker* ReceiverGameServerWorker;
	FRunnableThread* ReceiverGameServerThread;

	NetworkSenderWorker* SenderGameServerWorker;
	FRunnableThread* SenderGameServerThread;

	// timers
	FTimerHandle NetworkLoginServerPollTimerHandle;
	FTimerHandle NetworkGameServerPollTimerHandle;


public:
	UNetworkManager(const FObjectInitializer& ObjectInitializer);
	void InitializeTCPConnection();
	void SetWorldContext(UWorld* World);
	void StartPollingLoginServer();
	void StartPollingGameServer();
	void SendDataToLoginServer(const FString& Data);
	void SendDataToGameServer(const FString& Data);
	void PollLoginServerNetworkData();
	void PollGameServerNetworkData();
	void Shutdown();

	// Delegate instance
	FOnLoginServerDataReceived OnLoginServerDataReceived;
	FOnGameServerDataReceived OnGameServerDataReceived;
};
