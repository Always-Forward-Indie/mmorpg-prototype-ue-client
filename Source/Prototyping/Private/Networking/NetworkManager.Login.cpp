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

//start connection to login server
void UNetworkManager::ConnectLoginServer()
{
	// ����������� �����, ���� �� ��� �� ������
	if (!LoginServerSocket)
	{
		LoginServerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("LoginServerSocket"), false);
		LoginServerSocket->SetNonBlocking(true);
		LoginServerSocket->SetReuseAddr(true);
		LoginServerSocket->SetRecvErr(true);
		LoginServerSocket->SetNoDelay(true);
	}

	// ������������� IP � ����
	FIPv4Address LoginServerIPAddr;
	FIPv4Address::Parse(LoginServerIP, LoginServerIPAddr);
	FIPv4Endpoint LoginServerEndpoint(LoginServerIPAddr, LoginServerPort);

	// ��������� �����������
	bIsLoginSocketConnected = LoginServerSocket->Connect(*LoginServerEndpoint.ToInternetAddr());
	LoginConnectionRetryCount = 0;

	// ��������� ������ ��� �������� �����������
	TWeakObjectPtr<UNetworkManager> WeakThis(this);
	FTimerDelegate LoginTimerDelegate;
	LoginTimerDelegate.BindWeakLambda(this, [WeakThis, LoginServerEndpoint]()
		{
			UNetworkManager* Mgr = WeakThis.Get();
			if (!Mgr || Mgr->bIsShutDown || !Mgr->WorldContext) return;

			if (Mgr->LoginServerSocket && Mgr->LoginServerSocket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
			{
				if (Mgr->ReceiverLoginServerWorker) { return; }

				UE_LOG(LogConnection, Log, TEXT("Login Server socket connected."));

				if (Mgr->WorldContext) { Mgr->WorldContext->GetTimerManager().ClearTimer(Mgr->LoginServerConnectionTimerHandle); }
				Mgr->LoginServerConnectionTimerHandle.Invalidate();

				Mgr->ReceiverLoginServerWorker = new NetworkReceiverWorker(Mgr->LoginServerSocket);
				Mgr->ReceiverLoginServerWorker->SetSocketTag(TEXT("Login"));
				if (UTimeSyncService* TimeSyncSvc = Mgr->GetTimeSyncService())
				{
					Mgr->ReceiverLoginServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				Mgr->ReceiverLoginServerThread = FRunnableThread::Create(Mgr->ReceiverLoginServerWorker,
					*FString::Printf(TEXT("NetLoginRecv_%d"), Mgr->InstanceId));

				Mgr->SenderLoginServerWorker = new NetworkSenderWorker(Mgr->LoginServerSocket);
				if (UTimeSyncService* TimeSyncSvc = Mgr->GetTimeSyncService())
				{
					Mgr->SenderLoginServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				Mgr->SenderLoginServerThread = FRunnableThread::Create(Mgr->SenderLoginServerWorker,
					*FString::Printf(TEXT("NetLoginSend_%d"), Mgr->InstanceId));
			
				Mgr->OnLoginServerSocketConnected.Broadcast();
			}
			else
			{
				Mgr->LoginConnectionRetryCount++;
				UE_LOG(LogConnection, Verbose, TEXT("Waiting for Login Server socket connection... Retry %d"), Mgr->LoginConnectionRetryCount);
				if (FRetryPolicy::HasExceededMaxRetries(Mgr->LoginConnectionRetryCount, Mgr->MaxLoginRetries))
				{
					UE_LOG(LogConnection, Error, TEXT("Failed to connect to Login Server after %d retries."), Mgr->LoginConnectionRetryCount);
					if (Mgr->WorldContext) { Mgr->WorldContext->GetTimerManager().ClearTimer(Mgr->LoginServerConnectionTimerHandle); }
					if (Mgr->WorldContext) { Mgr->ShowLoginServerConnectionIssuePopup(); }
				}
			}
		});
	WorldContext->GetTimerManager().SetTimer(LoginServerConnectionTimerHandle, LoginTimerDelegate, FRetryPolicy::ConnectionRetryIntervalSec, true);
}

