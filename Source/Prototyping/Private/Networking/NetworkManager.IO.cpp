// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/ChunkLinkRule.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"

void UNetworkManager::SendDataToLoginServer(const FString& Data) {
	SendViaLink(EServerType::LoginServer, Data);
}

bool UNetworkManager::IsLoginServerConnected() const
{
	return IsLinkConnected(EServerType::LoginServer);
}

bool UNetworkManager::IsGameServerConnected() const
{
	return IsLinkConnected(EServerType::GameServer);
}

bool UNetworkManager::IsChunkServerConnected() const
{
	return IsLinkConnected(EServerType::ChunkServer);
}

void UNetworkManager::SendDataToGameServer(const FString& Data) {
	SendViaLink(EServerType::GameServer, Data);
}

// send data to chunk server
void UNetworkManager::SendDataToChunkServer(const FString& Data)
{
	SendViaLink(EServerType::ChunkServer, Data);
}

void UNetworkManager::PollLoginServerNetworkData()
{
	PollLink(EServerType::LoginServer);
}

void UNetworkManager::PollGameServerNetworkData()
{
	PollLink(EServerType::GameServer);
}

// poll data from chunk server
void UNetworkManager::PollChunkServerNetworkData()
{
	PollLink(EServerType::ChunkServer);
}
