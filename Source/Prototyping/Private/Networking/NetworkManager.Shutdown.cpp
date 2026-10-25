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

void UNetworkManager::Disconnect()
{
	UE_LOG(LogConnection, Log, TEXT("Disconnecting Network Manager..."));

	// Step 1: stop the core ticker so TickLinks stops firing immediately.
	// This prevents PollLink from touching worker pointers while we tear
	// them down below (same role the old ClearTimer block had).
	if (PollTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollTickerHandle);
		PollTickerHandle.Reset();
	}

	// Step 2: Give sender threads a brief window (~200 ms) to drain any
	// disconnect packets that were just enqueued (e.g. SendLeaveGameRequest).
	FPlatformProcess::Sleep(0.2f);

	// Steps 3-6 per link (Stop -> Detach -> Close -> WaitForCompletion ->
	// Destroy -> delete), same safe order as before.
	TeardownLink(EServerType::LoginServer);
	TeardownLink(EServerType::GameServer);
	TeardownLink(EServerType::ChunkServer);

	UE_LOG(LogConnection, Log, TEXT("Network Manager disconnected."));
}

void UNetworkManager::Shutdown()
{
	if (bIsShutDown)
	{
		return;
	}
	bIsShutDown = true;

	UE_LOG(LogConnection, Log, TEXT("Shutting down Network Manager..."));
	Disconnect();
	UE_LOG(LogConnection, Log, TEXT("Network Manager shut down complete."));
}

UNetworkManager::~UNetworkManager()
{
	// Ensure clean shutdown if Shutdown() was not called explicitly
	Shutdown();
}

void UNetworkManager::ProcessIncomingData(const FString& ReceivedData)
{
	ProcessIncomingPacket(JSONParser::ParsePacket(ReceivedData));
}

void UNetworkManager::ProcessIncomingPacket(const FParsedPacket& Packet)
{
	if (UTimeSyncService* TimeSyncService = GetTimeSyncService())
	{
		JSONParser::ProcessTimeSyncFromPacket(Packet, TimeSyncService);
	}
}
