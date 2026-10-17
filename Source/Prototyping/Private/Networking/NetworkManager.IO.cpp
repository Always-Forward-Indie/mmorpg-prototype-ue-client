// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonUtilities.h"
#include <Kismet/KismetSystemLibrary.h>
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/RetryPolicy.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"

void UNetworkManager::SendDataToLoginServer(const FString& Data) {
	if (SenderLoginServerWorker != nullptr)
	{
		SenderLoginServerWorker->EnqueueDataForSending(Data);
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("Sender Login Server Worker is null"));
	}
}

bool UNetworkManager::IsLoginServerConnected() const
{
	return SenderLoginServerWorker != nullptr;
}

bool UNetworkManager::IsGameServerConnected() const
{
	return SenderGameServerWorker != nullptr;
}

bool UNetworkManager::IsChunkServerConnected() const
{
	return SenderChunkServerWorker != nullptr;
}

void UNetworkManager::SendDataToGameServer(const FString& Data) {
	if (SenderGameServerWorker != nullptr)
	{
		SenderGameServerWorker->EnqueueDataForSending(Data);
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("Sender Game Server Worker is null"));
	}
}

// send data to chunk server
void UNetworkManager::SendDataToChunkServer(const FString& Data)
{
	if (SenderChunkServerWorker != nullptr)
	{
		SenderChunkServerWorker->EnqueueDataForSending(Data);
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("Sender Chunk Server Worker is null"));
	}
}

void UNetworkManager::PollLoginServerNetworkData()
{
	CRASH_GUARD("NetworkManager::PollLoginServer");

	FString ReceivedData;
	while (ReceiverLoginServerWorker && ReceiverLoginServerWorker->GetData(ReceivedData))
	{
		UE_LOG(LogNetPacket, Verbose, TEXT("Received Login Server data: %s"), *ReceivedData);

		// Process time sync data before broadcasting
		ProcessIncomingData(ReceivedData);

		// Trigger the event or delegate call
		OnLoginServerDataReceived.Broadcast(ReceivedData);
	}
}

void UNetworkManager::PollGameServerNetworkData()
{
	CRASH_GUARD("NetworkManager::PollGameServer");

	FString ReceivedData;
	while (ReceiverGameServerWorker && ReceiverGameServerWorker->GetData(ReceivedData))
	{
		UE_LOG(LogNetPacket, Verbose, TEXT("Received Game Server data: %s"), *ReceivedData);

		// Process time sync data before broadcasting
		ProcessIncomingData(ReceivedData);

		// Trigger the event or delegate call
		OnGameServerDataReceived.Broadcast(ReceivedData);
	}
}

// poll data from chunk server
void UNetworkManager::PollChunkServerNetworkData()
{
	CRASH_GUARD("NetworkManager::PollChunkServer");

	FString ReceivedData;
	while (ReceiverChunkServerWorker && ReceiverChunkServerWorker->GetData(ReceivedData))
	{
		UE_LOG(LogNetPacket, Verbose, TEXT("Received Chunk Server data: %s"), *ReceivedData);
		
		// Process time sync data before broadcasting
		ProcessIncomingData(ReceivedData);
		
		// Trigger the event or delegate call
		OnChunkServerDataReceived.Broadcast(ReceivedData);
	}
}

