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

	// Step 1: Clear all timers so poll callbacks stop firing immediately.
	// This prevents PollXxxNetworkData() from touching worker pointers while
	// we are tearing them down below.
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkLoginServerPollTimerHandle);
		WorldContext->GetTimerManager().ClearTimer(NetworkGameServerPollTimerHandle);
		WorldContext->GetTimerManager().ClearTimer(NetworkChunkServerPollTimerHandle);
		WorldContext->GetTimerManager().ClearTimer(LoginServerConnectionTimerHandle);
		WorldContext->GetTimerManager().ClearTimer(GameServerConnectionTimerHandle);
		WorldContext->GetTimerManager().ClearTimer(ChunkServerConnectionTimerHandle);
	}

	// Step 2: Give sender threads a brief window (~200 ms) to drain any
	// disconnect packets that were just enqueued (e.g. SendLeaveGameRequest).
	FPlatformProcess::Sleep(0.2f);

	// Stop ALL workers before touching sockets. This sets bRunThread = false
	// so every Run() loop exits at its next iteration boundary.
	if (SenderLoginServerWorker)    SenderLoginServerWorker->Stop();
	if (SenderGameServerWorker)     SenderGameServerWorker->Stop();
	if (SenderChunkServerWorker)    SenderChunkServerWorker->Stop();
	if (ReceiverLoginServerWorker)  ReceiverLoginServerWorker->Stop();
	if (ReceiverGameServerWorker)   ReceiverGameServerWorker->Stop();
	if (ReceiverChunkServerWorker)  ReceiverChunkServerWorker->Stop();

	// Step 3a: Detach socket pointers from all workers BEFORE calling Close().
	// DetachSocket() nulls the Socket member so workers see nullptr and break.
	if (ReceiverLoginServerWorker) ReceiverLoginServerWorker->DetachSocket();
	if (SenderLoginServerWorker)   SenderLoginServerWorker->DetachSocket();
	if (ReceiverGameServerWorker)  ReceiverGameServerWorker->DetachSocket();
	if (SenderGameServerWorker)    SenderGameServerWorker->DetachSocket();
	if (ReceiverChunkServerWorker) ReceiverChunkServerWorker->DetachSocket();
	if (SenderChunkServerWorker)   SenderChunkServerWorker->DetachSocket();

	// Step 3b: Close sockets to unblock any worker stuck in Recv().
	// Do NOT destroy yet — a worker may still hold a captured copy of the
	// pointer and call a virtual method during its final iteration.
	// DestroySocket() frees the FSocket vtable; calling a virtual on freed
	// memory produces the _purecall / EXCEPTION_ACCESS_VIOLATION crash.
	if (LoginServerSocket != nullptr)
	{
		LoginServerSocket->Close();
	}
	if (GameServerSocket != nullptr)
	{
		GameServerSocket->Close();
	}
	if (ChunkServerSocket != nullptr)
	{
		ChunkServerSocket->Close();
	}

	// Step 4: Wait for all threads to exit cleanly. With bRunThread == false
	// and Socket == nullptr every worker will break out of its Run() loop.
	// Any Recv() that was blocked is now unblocked by Close() above.
	auto ShutdownThread = [](FRunnable* Worker, FRunnableThread*& Thread)
	{
		if (Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}
	};

	ShutdownThread(ReceiverLoginServerWorker, ReceiverLoginServerThread);
	ShutdownThread(SenderLoginServerWorker,   SenderLoginServerThread);
	ShutdownThread(ReceiverGameServerWorker,  ReceiverGameServerThread);
	ShutdownThread(SenderGameServerWorker,    SenderGameServerThread);
	ShutdownThread(ReceiverChunkServerWorker, ReceiverChunkServerThread);
	ShutdownThread(SenderChunkServerWorker,   SenderChunkServerThread);

	// Step 5: NOW it is safe to destroy sockets. Every worker thread has
	// fully exited, so no thread can call a virtual method on the FSocket.
	if (LoginServerSocket != nullptr)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(LoginServerSocket);
		LoginServerSocket = nullptr;
	}
	if (GameServerSocket != nullptr)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(GameServerSocket);
		GameServerSocket = nullptr;
	}
	if (ChunkServerSocket != nullptr)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ChunkServerSocket);
		ChunkServerSocket = nullptr;
	}

	// Step 6: Delete worker objects — all threads have fully exited.
	delete ReceiverLoginServerWorker; ReceiverLoginServerWorker = nullptr;
	delete SenderLoginServerWorker;   SenderLoginServerWorker   = nullptr;
	delete ReceiverGameServerWorker;  ReceiverGameServerWorker  = nullptr;
	delete SenderGameServerWorker;    SenderGameServerWorker    = nullptr;
	delete ReceiverChunkServerWorker; ReceiverChunkServerWorker = nullptr;
	delete SenderChunkServerWorker;   SenderChunkServerWorker   = nullptr;

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
	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);

	if (UTimeSyncService* TimeSyncService = GetTimeSyncService())
	{
		JSONParser::ProcessTimeSyncFromHeader(ReceivedData, TimeSyncService);
	}
}
