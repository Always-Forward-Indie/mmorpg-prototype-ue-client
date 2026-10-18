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

void UNetworkManager::ConnectGameServer()
{
	// ����������� ������ ��� �������� �������
	if (!GameServerSocket)
	{
		GameServerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("GameServerSocket"), false);
		GameServerSocket->SetNonBlocking(true);
		GameServerSocket->SetReuseAddr(true);
		GameServerSocket->SetRecvErr(true);
		GameServerSocket->SetNoDelay(true);
	}

	FIPv4Address GameServerIPAddr;
	FIPv4Address::Parse(GameServerIP, GameServerIPAddr);
	FIPv4Endpoint GameServerEndpoint(GameServerIPAddr, GameServerPort);

	bIsGameSocketConnected = GameServerSocket->Connect(*GameServerEndpoint.ToInternetAddr());
	GameConnectionRetryCount = 0;

	TWeakObjectPtr<UNetworkManager> WeakThis(this);
	FTimerDelegate GameTimerDelegate;
	GameTimerDelegate.BindWeakLambda(this, [WeakThis, GameServerEndpoint]()
		{
			UNetworkManager* Mgr = WeakThis.Get();
			if (!Mgr || Mgr->bIsShutDown || !Mgr->WorldContext) return;

			if (Mgr->GameServerSocket && Mgr->GameServerSocket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
			{
				if (Mgr->ReceiverGameServerWorker) { return; }

				UE_LOG(LogConnection, Log, TEXT("Game Server socket connected."));

				if (Mgr->WorldContext) { Mgr->WorldContext->GetTimerManager().ClearTimer(Mgr->GameServerConnectionTimerHandle); }
				Mgr->GameServerConnectionTimerHandle.Invalidate();

				Mgr->ReceiverGameServerWorker = new NetworkReceiverWorker(Mgr->GameServerSocket);
				Mgr->ReceiverGameServerWorker->SetSocketTag(TEXT("Game"));
				if (UTimeSyncService* TimeSyncSvc = Mgr->GetTimeSyncService())
				{
					Mgr->ReceiverGameServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				Mgr->ReceiverGameServerThread = FRunnableThread::Create(Mgr->ReceiverGameServerWorker,
					*FString::Printf(TEXT("NetGameRecv_%d"), Mgr->InstanceId));

				Mgr->SenderGameServerWorker = new NetworkSenderWorker(Mgr->GameServerSocket);
				if (UTimeSyncService* TimeSyncSvc = Mgr->GetTimeSyncService())
				{
					Mgr->SenderGameServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				Mgr->SenderGameServerThread = FRunnableThread::Create(Mgr->SenderGameServerWorker,
					*FString::Printf(TEXT("NetGameSend_%d"), Mgr->InstanceId));

				Mgr->OnGameServerSocketConnected.Broadcast();
			}
			else
			{
				Mgr->GameConnectionRetryCount++;
				UE_LOG(LogConnection, Verbose, TEXT("Waiting for Game Server socket connection... Retry %d"), Mgr->GameConnectionRetryCount);
				if (FRetryPolicy::HasExceededMaxRetries(Mgr->GameConnectionRetryCount, Mgr->MaxGameRetries))
				{
					UE_LOG(LogConnection, Error, TEXT("Failed to connect to Game Server after %d retries."), Mgr->GameConnectionRetryCount);
					if (Mgr->WorldContext) { Mgr->WorldContext->GetTimerManager().ClearTimer(Mgr->GameServerConnectionTimerHandle); }
					if (Mgr->WorldContext) { Mgr->ShowGameServerConnectionIssuePopup(); }
				}
			}
		});
	WorldContext->GetTimerManager().SetTimer(GameServerConnectionTimerHandle, GameTimerDelegate, FRetryPolicy::ConnectionRetryIntervalSec, true);
}

