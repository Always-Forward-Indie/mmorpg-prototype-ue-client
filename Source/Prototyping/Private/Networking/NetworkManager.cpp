// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonUtilities.h"
#include <Kismet/KismetSystemLibrary.h>
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Prototyping.h"

// File-scope counter � atomically incremented for each new UNetworkManager instance.
// Kept here (not in the header) so UHT never sees it.
static int32 GNetworkManagerNextInstanceId = 0;

UNetworkManager::UNetworkManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceId = FPlatformAtomics::InterlockedIncrement(&GNetworkManagerNextInstanceId);
	UE_LOG(LogConnection, Log, TEXT("Network Manager Constructor called"));

	FString ConfigFilePath;

	// Detect whether we are running in the Unreal Editor or a packaged game
	if (FPaths::FileExists(FPaths::ProjectDir() + TEXT("server_config.json")))
	{
		// If running in Unreal Editor, use the project's root folder
		ConfigFilePath = FPaths::ProjectDir() + TEXT("server_config.json");
	}
	else
	{
		// If running a packaged game, use the same directory as the executable
		ConfigFilePath = FPaths::LaunchDir() + TEXT("server_config.json");
	}

	UE_LOG(LogConnection, Log, TEXT("Config file path: %s"), *ConfigFilePath);

	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *ConfigFilePath))
	{
		UE_LOG(LogConnection, Log, TEXT("Successfully loaded config file from: %s"), *ConfigFilePath);

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			LoginServerIP = JsonObject->GetObjectField(TEXT("LoginServer"))->GetStringField(TEXT("IP"));
			LoginServerPort = JsonObject->GetObjectField(TEXT("LoginServer"))->GetIntegerField(TEXT("Port"));

			GameServerIP = JsonObject->GetObjectField(TEXT("GameServer"))->GetStringField(TEXT("IP"));
			GameServerPort = JsonObject->GetObjectField(TEXT("GameServer"))->GetIntegerField(TEXT("Port"));

			ChunkServerIP = JsonObject->GetObjectField(TEXT("ChunkServer"))->GetStringField(TEXT("IP"));
			ChunkServerPort = JsonObject->GetObjectField(TEXT("ChunkServer"))->GetIntegerField(TEXT("Port"));

			UE_LOG(LogConnection, Log, TEXT("Login Server: %s:%d"), *LoginServerIP, LoginServerPort);
			UE_LOG(LogConnection, Log, TEXT("Game Server: %s:%d"), *GameServerIP, GameServerPort);
			UE_LOG(LogConnection, Log, TEXT("Chunk Server: %s:%d"), *ChunkServerIP, ChunkServerPort);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON config file."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load config file: %s"), *ConfigFilePath);
	}
}


void UNetworkManager::SetWorldContext(UWorld* World) {
	WorldContext = World; // Set the world context
}

void UNetworkManager::SetMessageBoxPopupClass(TSubclassOf<UMessageBoxPopup> InMessageBoxPopupClass)
{
	MessageBoxPopupClass = InMessageBoxPopupClass;
}

void UNetworkManager::StartPollingLoginServer()
{
	const float PollIntervalLoginServerData = 0.0001f; // Adjust as necessary
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkLoginServerPollTimerHandle);
		WorldContext->GetTimerManager().SetTimer(NetworkLoginServerPollTimerHandle, this, &UNetworkManager::PollLoginServerNetworkData, PollIntervalLoginServerData, true);
		UE_LOG(LogConnection, Verbose, TEXT("Polling timer for login server data set up successfully."));
	}
}

void UNetworkManager::StartPollingGameServer()
{
	const float PollIntervalGameServerData = 0.001f; // Adjust as necessary
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkGameServerPollTimerHandle);
		WorldContext->GetTimerManager().SetTimer(NetworkGameServerPollTimerHandle, this, &UNetworkManager::PollGameServerNetworkData, PollIntervalGameServerData, true);
		UE_LOG(LogConnection, Verbose, TEXT("Polling timer for game server data set up successfully."));
	}
}

// start polling data from chunk server
void UNetworkManager::StartPollingChunkServer()
{
	const float PollIntervalChunkServerData = 0.001f; // Adjust as necessary
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkChunkServerPollTimerHandle);
		WorldContext->GetTimerManager().SetTimer(NetworkChunkServerPollTimerHandle, this, &UNetworkManager::PollChunkServerNetworkData, PollIntervalChunkServerData, true);
		UE_LOG(LogConnection, Verbose, TEXT("Polling timer for chunk server data set up successfully."));
	}
}

void UNetworkManager::RestartPolling()
{
	if (!WorldContext)
	{
		UE_LOG(LogConnection, Error, TEXT("NetworkManager::RestartPolling - WorldContext is null, cannot restart polling timers"));
		return;
	}

	UE_LOG(LogConnection, Log, TEXT("NetworkManager::RestartPolling - Re-registering poll timers in new world"));

	// The old TimerManager is gone after level transition - register fresh timers
	// in the new world's TimerManager. ClearTimer on a stale handle is a no-op so
	// it is safe to call even if the handle references the destroyed old world.
	NetworkLoginServerPollTimerHandle.Invalidate();
	NetworkGameServerPollTimerHandle.Invalidate();
	NetworkChunkServerPollTimerHandle.Invalidate();

	StartPollingLoginServer();
	StartPollingGameServer();
	StartPollingChunkServer();
}

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
	FTimerDelegate LoginTimerDelegate;
	LoginTimerDelegate.BindLambda([this, LoginServerEndpoint]()
		{
			if (LoginServerSocket && LoginServerSocket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
			{
				// Already set up � timer may fire again if ClearTimer was skipped
				if (ReceiverLoginServerWorker) { return; }

				UE_LOG(LogConnection, Log, TEXT("Login Server socket connected."));

				if (WorldContext) { WorldContext->GetTimerManager().ClearTimer(LoginServerConnectionTimerHandle); }
				LoginServerConnectionTimerHandle.Invalidate();

				// ������� ������ ��� ������ � �����-��������
				ReceiverLoginServerWorker = new NetworkReceiverWorker(LoginServerSocket);
				// Set TimeSyncService reference for precise timestamps
				if (UTimeSyncService* TimeSyncSvc = GetTimeSyncService())
				{
					ReceiverLoginServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				ReceiverLoginServerThread = FRunnableThread::Create(ReceiverLoginServerWorker,
					*FString::Printf(TEXT("NetLoginRecv_%d"), InstanceId));

				SenderLoginServerWorker = new NetworkSenderWorker(LoginServerSocket);
				// Set TimeSyncService reference for precise timestamps
				if (UTimeSyncService* TimeSyncSvc = GetTimeSyncService())
				{
					SenderLoginServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				SenderLoginServerThread = FRunnableThread::Create(SenderLoginServerWorker,
					*FString::Printf(TEXT("NetLoginSend_%d"), InstanceId));
			
				OnLoginServerSocketConnected.Broadcast();
			}
			else
			{
				LoginConnectionRetryCount++;
				UE_LOG(LogConnection, Verbose, TEXT("Waiting for Login Server socket connection... Retry %d"), LoginConnectionRetryCount);
				if (LoginConnectionRetryCount > MaxLoginRetries)
				{
					UE_LOG(LogConnection, Error, TEXT("Failed to connect to Login Server after %d retries."), LoginConnectionRetryCount);
					if (WorldContext) { WorldContext->GetTimerManager().ClearTimer(LoginServerConnectionTimerHandle); }
					// ���������� ����� � �������: ��������� ��� �����
					ShowLoginServerConnectionIssuePopup();
				}
			}
		});
	WorldContext->GetTimerManager().SetTimer(LoginServerConnectionTimerHandle, LoginTimerDelegate, 1.0f, true);
}

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

	FTimerDelegate GameTimerDelegate;
	GameTimerDelegate.BindLambda([this, GameServerEndpoint]()
		{
			if (GameServerSocket && GameServerSocket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
			{
				// Already set up � timer may fire again if ClearTimer was skipped
				if (ReceiverGameServerWorker) { return; }

				UE_LOG(LogConnection, Log, TEXT("Game Server socket connected."));

				if (WorldContext) { WorldContext->GetTimerManager().ClearTimer(GameServerConnectionTimerHandle); }
				GameServerConnectionTimerHandle.Invalidate();

				// ������� ������ ��� ������ � ������� ��������
				ReceiverGameServerWorker = new NetworkReceiverWorker(GameServerSocket);
				// Set TimeSyncService reference for precise timestamps
				if (UTimeSyncService* TimeSyncSvc = GetTimeSyncService())
				{
					ReceiverGameServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				ReceiverGameServerThread = FRunnableThread::Create(ReceiverGameServerWorker,
					*FString::Printf(TEXT("NetGameRecv_%d"), InstanceId));

				SenderGameServerWorker = new NetworkSenderWorker(GameServerSocket);
				// Set TimeSyncService reference for precise timestamps
				if (UTimeSyncService* TimeSyncSvc = GetTimeSyncService())
				{
					SenderGameServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				SenderGameServerThread = FRunnableThread::Create(SenderGameServerWorker,
					*FString::Printf(TEXT("NetGameSend_%d"), InstanceId));

				OnGameServerSocketConnected.Broadcast();
			}
			else
			{
				GameConnectionRetryCount++;
				UE_LOG(LogConnection, Verbose, TEXT("Waiting for Game Server socket connection... Retry %d"), GameConnectionRetryCount);
				if (GameConnectionRetryCount > MaxGameRetries)
				{
					UE_LOG(LogConnection, Error, TEXT("Failed to connect to Game Server after %d retries."), GameConnectionRetryCount);
					if (WorldContext) { WorldContext->GetTimerManager().ClearTimer(GameServerConnectionTimerHandle); }
					// ����� �������� ����������� ����� ��� �������� �������, ���� �����
					ShowGameServerConnectionIssuePopup();
				}
			}
		});
	WorldContext->GetTimerManager().SetTimer(GameServerConnectionTimerHandle, GameTimerDelegate, 1.0f, true);
}

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
	FTimerDelegate ChunkTimerDelegate;
	ChunkTimerDelegate.BindLambda([this, ChunkServerEndpoint]()
		{
			if (ChunkServerSocket && ChunkServerSocket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
			{
				// Already set up � timer may fire again if ClearTimer was skipped
				if (ReceiverChunkServerWorker) { return; }

				UE_LOG(LogConnection, Log, TEXT("Chunk Server socket connected."));

				if (WorldContext) { WorldContext->GetTimerManager().ClearTimer(ChunkServerConnectionTimerHandle); }
				ChunkServerConnectionTimerHandle.Invalidate();
				
				// ������� ������ ��� ������ � chunk ��������
				ReceiverChunkServerWorker = new NetworkReceiverWorker(ChunkServerSocket);
				// Set TimeSyncService reference for precise timestamps
				if (UTimeSyncService* TimeSyncSvc = GetTimeSyncService())
				{
					ReceiverChunkServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				ReceiverChunkServerThread = FRunnableThread::Create(ReceiverChunkServerWorker,
					*FString::Printf(TEXT("NetChunkRecv_%d"), InstanceId));

				SenderChunkServerWorker = new NetworkSenderWorker(ChunkServerSocket);
				// Set TimeSyncService reference for precise timestamps
				if (UTimeSyncService* TimeSyncSvc = GetTimeSyncService())
				{
					SenderChunkServerWorker->SetTimeSyncService(TimeSyncSvc);
				}
				SenderChunkServerThread = FRunnableThread::Create(SenderChunkServerWorker,
					*FString::Printf(TEXT("NetChunkSend_%d"), InstanceId));

				OnChunkServerSocketConnected.Broadcast();
			}
			else
			{
				ChunkConnectionRetryCount++;
				UE_LOG(LogConnection, Verbose, TEXT("Waiting for Chunk Server socket connection... Retry %d"), ChunkConnectionRetryCount);
				if (ChunkConnectionRetryCount > MaxChunkRetries)
				{
					UE_LOG(LogConnection, Error, TEXT("Failed to connect to Chunk Server after %d retries."), ChunkConnectionRetryCount);
					if (WorldContext) { WorldContext->GetTimerManager().ClearTimer(ChunkServerConnectionTimerHandle); }
					// ����� �������� ����������� ����� ��� �������� �������,
					// ���� �����
					ShowChunkServerConnectionIssuePopup();
				}
			}
		});
	WorldContext->GetTimerManager().SetTimer(ChunkServerConnectionTimerHandle, ChunkTimerDelegate, 1.0f, true);
}


UTimeSyncService* UNetworkManager::GetTimeSyncService()
{
    if (WorldContext)
    {
        UMyGameInstance* GameInstance = Cast<UMyGameInstance>(WorldContext->GetGameInstance());
        if (GameInstance)
        {
            return GameInstance->GetTimeSyncService();
        }
    }
    return nullptr;
}

// show popup for chunk server connection issue
void UNetworkManager::ShowChunkServerConnectionIssuePopup()
{
	if (MessageBoxPopupClass)
	{
		// ���� ���� ��� �������, �� ������ �����
		if (!MsgBoxChunkServer)
		{
			MsgBoxChunkServer = CreateWidget<UMessageBoxPopup>(WorldContext, MessageBoxPopupClass);
		}
		if (MsgBoxChunkServer)
		{
			FText TitleMessage = FText::FromString(TEXT("Error"));
			FText ErrorMessage = FText::FromString(TEXT("Can not connect to Chunk Server. Retry?"));
			FText YesText = FText::FromString(TEXT("Yes"));
			FText NoText = FText::FromString(TEXT("No"));
			MsgBoxChunkServer->SetupMessageBox(TitleMessage, ErrorMessage, YesText, NoText);
			// ������������� ���� �� �����
			MsgBoxChunkServer->AddToViewport();
			// ������������� �� ������� ������
			MsgBoxChunkServer->OnLeftButtonClicked.AddDynamic(this, &UNetworkManager::OnChunkServerConnectionRetry);
			MsgBoxChunkServer->OnRightButtonClicked.AddDynamic(this, &UNetworkManager::OnConnectCancel);
		}
	}
}

// show popup for login server connection issue
void UNetworkManager::ShowLoginServerConnectionIssuePopup()
{
	if (MessageBoxPopupClass)
	{
		// ���� ���� ��� �������, �� ������ �����
		if (!MsgBoxLoginServer)
		{
			MsgBoxLoginServer = CreateWidget<UMessageBoxPopup>(WorldContext, MessageBoxPopupClass);
		}
		if (MsgBoxLoginServer)
		{

			FText TitleMessage = FText::FromString(TEXT("Error"));
			FText ErrorMessage = FText::FromString(TEXT("Can not connect to Login Server. Retry?"));
			FText YesText = FText::FromString(TEXT("Yes"));
			FText NoText = FText::FromString(TEXT("No"));
			MsgBoxLoginServer->SetupMessageBox(TitleMessage, ErrorMessage, YesText, NoText);

			// ������������� ���� �� �����
			MsgBoxLoginServer->AddToViewport();

			// ������������� �� ������� ������
			MsgBoxLoginServer->OnLeftButtonClicked.AddDynamic(this, &UNetworkManager::OnLoginServerConnectionRetry);
			MsgBoxLoginServer->OnRightButtonClicked.AddDynamic(this, &UNetworkManager::OnConnectCancel);
		}
	}
}

void UNetworkManager::ShowGameServerConnectionIssuePopup()
{
	if (MessageBoxPopupClass)
	{
		// ���� ���� ��� �������, �� ������ �����
		if (!MsgBoxGameServer)
		{
			MsgBoxGameServer = CreateWidget<UMessageBoxPopup>(WorldContext, MessageBoxPopupClass);
		}
		if (MsgBoxGameServer)
		{
			FText TitleMessage = FText::FromString(TEXT("Error"));
			FText ErrorMessage = FText::FromString(TEXT("Can not connect to Game Server. Retry?"));
			FText YesText = FText::FromString(TEXT("Yes"));
			FText NoText = FText::FromString(TEXT("No"));
			MsgBoxGameServer->SetupMessageBox(TitleMessage, ErrorMessage, YesText, NoText);

			// ������������� ���� �� �����
			MsgBoxGameServer->AddToViewport();

			// ������������� �� ������� ������
			MsgBoxGameServer->OnLeftButtonClicked.AddDynamic(this, &UNetworkManager::OnGameServerConnectionRetry);
			MsgBoxGameServer->OnRightButtonClicked.AddDynamic(this, &UNetworkManager::OnConnectCancel);
		}
	}
}


void UNetworkManager::OnLoginServerConnectionRetry()
{
	if (MsgBoxLoginServer)
	{
		MsgBoxLoginServer->OnLeftButtonClicked.Clear();
		MsgBoxLoginServer->OnRightButtonClicked.Clear();
		MsgBoxLoginServer->RemoveFromParent();
		MsgBoxLoginServer = nullptr;
	}

	UE_LOG(LogConnection, Log, TEXT("User chose to retry Login Server connection."));
	// ������� ���������������: �������� ����� ����������� ������
	ConnectLoginServer();
}

void UNetworkManager::OnGameServerConnectionRetry()
{
	UE_LOG(LogConnection, Log, TEXT("User chose to retry Game Server connection."));
	if (MsgBoxGameServer) 
	{
		MsgBoxGameServer->OnLeftButtonClicked.Clear();
		MsgBoxGameServer->OnRightButtonClicked.Clear();
		MsgBoxGameServer->RemoveFromParent();
		MsgBoxGameServer = nullptr;
	}

	// ������� ���������������: �������� ����� ����������� ������
	ConnectGameServer();
}

// retry connection to chunk server
void UNetworkManager::OnChunkServerConnectionRetry()
{
	if (MsgBoxChunkServer)
	{
		MsgBoxChunkServer->OnLeftButtonClicked.Clear();
		MsgBoxChunkServer->OnRightButtonClicked.Clear();
		MsgBoxChunkServer->RemoveFromParent();
		MsgBoxChunkServer = nullptr;
	}
	UE_LOG(LogConnection, Log, TEXT("User chose to retry Chunk Server connection."));
	// ������� ���������������: �������� ����� ����������� ������
	ConnectChunkServer();
}


// if cancel connection
void UNetworkManager::OnConnectCancel()
{
	UE_LOG(LogConnection, Log, TEXT("User cancelled Server connection."));
	// ��������� ���� ��� ��������� ������ ������ ������
	UKismetSystemLibrary::QuitGame(WorldContext, nullptr, EQuitPreference::Quit, true);
}


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
	//UE_LOG(LogTemp, Warning, TEXT("Polling for Login Server data..."));
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
	//UE_LOG(LogTemp, Warning, TEXT("Polling for Game Server data..."));
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
	//UE_LOG(LogTemp, Warning, TEXT("Polling for Chunk Server data..."));
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

void UNetworkManager::Shutdown() {

	if (bIsShutDown)
	{
		return;
	}
	bIsShutDown = true;

	UE_LOG(LogConnection, Log, TEXT("Shutting down Network Manager..."));

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
	// disconnect packets that were just enqueued by Shutdown() callers
	// (e.g. SendLeaveGameRequest).  The senders are still running at this
	// point (bRunThread == true), so they will dequeue and send the data.
	// Only AFTER this drain window do we signal Stop().
	FPlatformProcess::Sleep(0.2f);

	if (SenderLoginServerWorker)  SenderLoginServerWorker->Stop();
	if (SenderGameServerWorker)   SenderGameServerWorker->Stop();
	if (SenderChunkServerWorker)  SenderChunkServerWorker->Stop();

	// Brief additional sleep so any in-flight send completes before socket close.
	FPlatformProcess::Sleep(0.05f);

	// Step 3a: Detach socket pointers from all workers BEFORE calling DestroySocket().
	// DestroySocket() frees the FSocket object and corrupts its vtable.
	// If a worker thread reads the Socket pointer between Close() and DestroySocket()
	// it would call a virtual method through a dangling vtable ? crash 0xFFFFFFFFFFFFFFFF.
	// DetachSocket() atomically nulls the pointer so workers see nullptr and exit cleanly.
	if (ReceiverLoginServerWorker) ReceiverLoginServerWorker->DetachSocket();
	if (SenderLoginServerWorker)   SenderLoginServerWorker->DetachSocket();
	if (ReceiverGameServerWorker)  ReceiverGameServerWorker->DetachSocket();
	if (SenderGameServerWorker)    SenderGameServerWorker->DetachSocket();
	if (ReceiverChunkServerWorker) ReceiverChunkServerWorker->DetachSocket();
	if (SenderChunkServerWorker)   SenderChunkServerWorker->DetachSocket();

	// Step 3b: Close and destroy sockets � workers already hold nullptr so they
	// cannot race against DestroySocket() anymore.
	if (LoginServerSocket != nullptr)
	{
		LoginServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(LoginServerSocket);
		LoginServerSocket = nullptr;
	}

	if (GameServerSocket != nullptr)
	{
		GameServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(GameServerSocket);
		GameServerSocket = nullptr;
	}

	if (ChunkServerSocket != nullptr)
	{
		ChunkServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ChunkServerSocket);
		ChunkServerSocket = nullptr;
	}

	// Step 4: Signal receiver threads to stop (sockets already closed so Recv
	// will return immediately) and wait for all threads to exit cleanly.
	auto ShutdownThread = [](FRunnable* Worker, FRunnableThread*& Thread)
	{
		if (Worker) Worker->Stop();
		if (Thread)
		{
			// 2-second safety timeout � should exit almost immediately after socket close.
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

	// Step 5: Delete worker objects only after their threads have fully exited.
	delete ReceiverLoginServerWorker; ReceiverLoginServerWorker = nullptr;
	delete SenderLoginServerWorker;   SenderLoginServerWorker   = nullptr;
	delete ReceiverGameServerWorker;  ReceiverGameServerWorker  = nullptr;
	delete SenderGameServerWorker;    SenderGameServerWorker    = nullptr;
	delete ReceiverChunkServerWorker; ReceiverChunkServerWorker = nullptr;
	delete SenderChunkServerWorker;   SenderChunkServerWorker   = nullptr;

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
