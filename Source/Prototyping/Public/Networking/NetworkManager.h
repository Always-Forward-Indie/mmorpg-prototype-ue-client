// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "Networking/NetworkReceiverWorker.h"
#include "Networking/NetworkSenderWorker.h"
#include <Gameplay/UI/MessageBoxPopup.h>
#include "NetworkManager.generated.h"



// Define a delegate or event signature
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoginServerDataReceived, const FString&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameServerDataReceived, const FString&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkServerDataReceived, const FString&, Data);


// Delegate to handle that Game Server socket connected
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameServerSocketConnected);

// Delegate to handle that Login Server socket connected
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoginServerSocketConnected);

// Delegate to handle that Chunk Server socket connected
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChunkServerSocketConnected);

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

	// Chunk server details
	FString ChunkServerIP;
	int32 ChunkServerPort;
	FSocket* ChunkServerSocket;
	bool bIsChunkSocketConnected;

	NetworkReceiverWorker* ReceiverChunkServerWorker;
	FRunnableThread* ReceiverChunkServerThread;

	NetworkSenderWorker* SenderChunkServerWorker;
	FRunnableThread* SenderChunkServerThread;

	// timers chunk server
	FTimerHandle NetworkChunkServerPollTimerHandle;
	FTimerHandle ChunkServerConnectionTimerHandle;

	int32 ChunkConnectionRetryCount = 0;
	const int32 MaxChunkRetries = 3;

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

	FTimerHandle LoginServerConnectionTimerHandle;
	FTimerHandle GameServerConnectionTimerHandle;

	int32 LoginConnectionRetryCount = 0;
	int32 GameConnectionRetryCount = 0;
	const int32 MaxLoginRetries = 3; 
	const int32 MaxGameRetries = 3; 

	// Класс виджета для месседж-бокса
	UPROPERTY()
	TSubclassOf<UMessageBoxPopup> MessageBoxPopupClass;

	UPROPERTY()
	UMessageBoxPopup* MsgBoxLoginServer;
	UPROPERTY()
	UMessageBoxPopup* MsgBoxGameServer;
	UPROPERTY()
	UMessageBoxPopup* MsgBoxChunkServer;


public:
	UNetworkManager(const FObjectInitializer& ObjectInitializer);
	void ConnectGameServer();
	void ShowLoginServerConnectionIssuePopup();
	void ShowGameServerConnectionIssuePopup();
	void ShowChunkServerConnectionIssuePopup();
	void SetWorldContext(UWorld* World);
	void SetMessageBoxPopupClass(TSubclassOf<UMessageBoxPopup> InMessageBoxPopupClass);
	void StartPollingLoginServer();
	void StartPollingGameServer();
	void StartPollingChunkServer();
	void ConnectLoginServer();
	void ConnectChunkServer();
	void SendDataToLoginServer(const FString& Data);
	void SendDataToGameServer(const FString& Data);
	void SendDataToChunkServer(const FString& Data);
	void PollLoginServerNetworkData();
	void PollGameServerNetworkData();
	void PollChunkServerNetworkData();
	void Shutdown();

	UFUNCTION()
	void OnLoginServerConnectionRetry();
	UFUNCTION()
	void OnGameServerConnectionRetry();
	UFUNCTION()
	void OnConnectCancel();
	UFUNCTION()
	void OnChunkServerConnectionRetry();

	// Delegate instance
	FOnLoginServerDataReceived OnLoginServerDataReceived;
	FOnGameServerDataReceived OnGameServerDataReceived;
	FOnChunkServerDataReceived OnChunkServerDataReceived;

	FOnGameServerSocketConnected OnGameServerSocketConnected;
	FOnLoginServerSocketConnected OnLoginServerSocketConnected;
	FOnChunkServerSocketConnected OnChunkServerSocketConnected;
};
