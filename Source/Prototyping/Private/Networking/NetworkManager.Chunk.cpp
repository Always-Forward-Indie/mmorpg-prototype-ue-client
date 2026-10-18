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
	// ����������� ������ ��� �������� �������
	if (!ChunkServerSocket)
	{
		ChunkServerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("ChunkServerSocket"), false);
		ChunkServerSocket->SetNonBlocking(true);
		ChunkServerSocket->SetReuseAddr(true);
		ChunkServerSocket->SetRecvErr(true);
		ChunkServerSocket->SetNoDelay(true);
	}
	FIPv4Address ChunkServerIPAddr;
	FIPv4Address::Parse(ChunkServerIP, ChunkServerIPAddr);
	FIPv4Endpoint ChunkServerEndpoint(ChunkServerIPAddr, ChunkServerPort);
	bIsChunkSocketConnected = ChunkServerSocket->Connect(*ChunkServerEndpoint.ToInternetAddr());
	ChunkConnectionRetryCount = 0;
	TWeakObjectPtr<UNetworkManager> WeakThis(this);
	FTimerDelegate ChunkTimerDelegate;
	ChunkTimerDelegate.BindWeakLambda(this, [WeakThis, ChunkServerEndpoint]()
		{
			UNetworkManager* Mgr = WeakThis.Get();
			if (!Mgr || Mgr->bIsShutDown || !Mgr->WorldContext) return;

			if (Mgr->ChunkServerSocket && Mgr->ChunkServerSocket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
			{
				if (Mgr->ReceiverChunkServerWorker) { return; }

				UE_LOG(LogConnection, Log, TEXT("Chunk Server socket connected."));

				if (Mgr->WorldContext) { Mgr->WorldContext->GetTimerManager().ClearTimer(Mgr->ChunkServerConnectionTimerHandle); }
				Mgr->ChunkServerConnectionTimerHandle.Invalidate();
				
				Mgr->ReceiverChunkServerWorker = new NetworkReceiverWorker(Mgr->ChunkServerSocket);
				Mgr->ReceiverChunkServerWorker->SetSocketTag(TEXT("Chunk"));
				if (UTimeSyncService* TimeSyncSvc = Mgr->GetTimeSyncService())
				{
					Mgr->ReceiverChunkServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				Mgr->ReceiverChunkServerThread = FRunnableThread::Create(Mgr->ReceiverChunkServerWorker,
					*FString::Printf(TEXT("NetChunkRecv_%d"), Mgr->InstanceId));

				Mgr->SenderChunkServerWorker = new NetworkSenderWorker(Mgr->ChunkServerSocket);
				if (UTimeSyncService* TimeSyncSvc = Mgr->GetTimeSyncService())
				{
					Mgr->SenderChunkServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				Mgr->SenderChunkServerThread = FRunnableThread::Create(Mgr->SenderChunkServerWorker,
					*FString::Printf(TEXT("NetChunkSend_%d"), Mgr->InstanceId));

				Mgr->OnChunkServerSocketConnected.Broadcast();
			}
			else
			{
				Mgr->ChunkConnectionRetryCount++;
				UE_LOG(LogConnection, Verbose, TEXT("Waiting for Chunk Server socket connection... Retry %d"), Mgr->ChunkConnectionRetryCount);
				if (FRetryPolicy::HasExceededMaxRetries(Mgr->ChunkConnectionRetryCount, Mgr->MaxChunkRetries))
				{
					UE_LOG(LogConnection, Error, TEXT("Failed to connect to Chunk Server after %d retries."), Mgr->ChunkConnectionRetryCount);
					if (Mgr->WorldContext) { Mgr->WorldContext->GetTimerManager().ClearTimer(Mgr->ChunkServerConnectionTimerHandle); }
					if (Mgr->WorldContext) { Mgr->ShowChunkServerConnectionIssuePopup(); }
				}
			}
		});
	WorldContext->GetTimerManager().SetTimer(ChunkServerConnectionTimerHandle, ChunkTimerDelegate, FRetryPolicy::ConnectionRetryIntervalSec, true);
}


