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

// connect to chunk server
void UNetworkManager::ConnectChunkServer()
{
	ConnectLink(EServerType::ChunkServer);
}

// Mid-session drop entry point (game thread, called from the poll timer).
// Guards: shutdown / no world / reconnect already running / no session yet
// (connect-time retries own that case). Otherwise tears the dead link down
// and dials again with the same budget ConnectChunkServer enforces.
void UNetworkManager::HandleChunkMidSessionDrop()
{
	FServerLink& Link = LinkFor(EServerType::ChunkServer);
	if (bIsShutDown || !WorldContext)
	{
		return;
	}
	if (Link.ConnectPending)
	{
		return;
	}
	if (!Link.Receiver && !Link.Sender)
	{
		return;
	}
	UE_LOG(LogConnection, Error, TEXT("Chunk mid-session drop detected - tearing down link and reconnecting (budget %d)."), Link.MaxRetries);
	TeardownChunkLink();
	ConnectChunkServer();
}

void UNetworkManager::TeardownChunkLink()
{
	// Chunk-only teardown via the unified path. The core ticker keeps
	// running on purpose: once ConnectChunkServer recreates the workers
	// the next tick picks the session back up.
	TeardownLink(EServerType::ChunkServer);

	UE_LOG(LogConnection, Log, TEXT("Chunk link torn down for reconnect."));
}

void UNetworkManager::DropChunkSocketForTest()
{
	FServerLink& Link = LinkFor(EServerType::ChunkServer);
	if (Link.Socket != nullptr)
	{
		UE_LOG(LogConnection, Warning, TEXT("DropChunkSocketForTest: closing chunk socket to simulate a mid-session drop."));
		Link.Socket->Close();
	}
	else
	{
		UE_LOG(LogConnection, Warning, TEXT("DropChunkSocketForTest: no chunk socket, nothing to drop."));
	}
}
