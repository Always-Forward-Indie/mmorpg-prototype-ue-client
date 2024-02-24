// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkManager.h"

UNetworkManager::UNetworkManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(LogTemp, Warning, TEXT("Network Manager Constructor called"));

	// Set up the login server IP and port
	LoginServerIP = TEXT("127.0.0.1");
	LoginServerPort = 27014;

	// Set up the game server IP and port
	GameServerIP = TEXT("127.0.0.1");
	GameServerPort = 27016;
}

void UNetworkManager::SetWorldContext(UWorld* World) {
	WorldContext = World; // Set the world context
}

void UNetworkManager::StartPollingLoginServer()
{
	const float PollIntervalLoginServerData = 0.001f; // Adjust as necessary
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkLoginServerPollTimerHandle);
		WorldContext->GetTimerManager().SetTimer(NetworkLoginServerPollTimerHandle, this, &UNetworkManager::PollLoginServerNetworkData, PollIntervalLoginServerData, true);
		UE_LOG(LogTemp, Warning, TEXT("Polling timer for login server data set up successfully."));
	}
}

void UNetworkManager::StartPollingGameServer()
{
	const float PollIntervalGameServerData = 0.001f; // Adjust as necessary
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkGameServerPollTimerHandle);
		WorldContext->GetTimerManager().SetTimer(NetworkGameServerPollTimerHandle, this, &UNetworkManager::PollGameServerNetworkData, PollIntervalGameServerData, true);
		UE_LOG(LogTemp, Warning, TEXT("Polling timer for game server data set up successfully."));
	}
}

void UNetworkManager::InitializeTCPConnection() {
	// Create the login server socket
	LoginServerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("LoginServerSocket"), false);
	LoginServerSocket->SetNonBlocking(true);
	LoginServerSocket->SetReuseAddr(true);
	LoginServerSocket->SetRecvErr(true);

	// Create the game server socket
	GameServerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("GameServerSocket"), false);
	GameServerSocket->SetNonBlocking(true);
	GameServerSocket->SetReuseAddr(true);
	GameServerSocket->SetRecvErr(true);
	//socket no delay
	GameServerSocket->SetNoDelay(true);


	if (LoginServerSocket == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Login Server Socket not created..."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Login Server Socket created"));
	}

	if (GameServerSocket == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Game Server Socket not created..."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Game Server Socket created"));
	}

	// Set the login server IP and port
	FIPv4Address LoginServerIPAddr;
	FIPv4Address::Parse(LoginServerIP, LoginServerIPAddr); // Replace with your server's IP
	FIPv4Endpoint LoginServerEndpoint(LoginServerIPAddr, LoginServerPort);

	// Set the game server IP and port
	FIPv4Address GameServerIPAddr;
	FIPv4Address::Parse(GameServerIP, GameServerIPAddr);// Replace with your server's IP
	FIPv4Endpoint GameServerEndpoint(GameServerIPAddr, GameServerPort);

	// Connect to the login server
	bIsLoginSocketConnected = LoginServerSocket->Connect(*LoginServerEndpoint.ToInternetAddr());

	// Connect to the game server
	bIsGameSocketConnected = GameServerSocket->Connect(*GameServerEndpoint.ToInternetAddr());

	if (!bIsLoginSocketConnected)
	{
		int32 ErrorCode = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode();
		UE_LOG(LogTemp, Error, TEXT("Login Server Socket not connected, Error Code: %d"), ErrorCode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Login Server Socket initialized"));

		// Create a thread to receive data from the server
		ReceiverLoginServerWorker = new NetworkReceiverWorker(LoginServerSocket);
		ReceiverLoginServerThread = FRunnableThread::Create(ReceiverLoginServerWorker, TEXT("NetworkingLoginServerReceiverThread"));

		if (ReceiverLoginServerThread != nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Receiver Login Server Thread created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Receiver Login Server Thread not created"));
		}

		// Create a thread to send data to the server
		SenderLoginServerWorker = new NetworkSenderWorker(LoginServerSocket);
		SenderLoginServerThread = FRunnableThread::Create(SenderLoginServerWorker, TEXT("NetworkingLoginServerSenderThread"));

		if (SenderLoginServerThread != nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Sender Login Server Thread created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Sender Login Server Thread not created"));
		}
	}


	if (!bIsGameSocketConnected)
	{
		int32 ErrorCode = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode();
		UE_LOG(LogTemp, Error, TEXT("Game Server Socket not connected, Error Code: %d"), ErrorCode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Game Server Socket initialized"));

		// Create a thread to receive data from the server
		ReceiverGameServerWorker = new NetworkReceiverWorker(GameServerSocket);
		ReceiverGameServerThread = FRunnableThread::Create(ReceiverGameServerWorker, TEXT("NetworkingGameServerReceiverThread"));

		if (ReceiverGameServerThread != nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Receiver Game Server Thread created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Receiver Game Server Thread not created"));
		}

		// Create a thread to send data to the server
		SenderGameServerWorker = new NetworkSenderWorker(GameServerSocket);
		SenderGameServerThread = FRunnableThread::Create(SenderGameServerWorker, TEXT("NetworkingGameServerSenderThread"));

		if (SenderGameServerThread != nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Sender Game Server Thread created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Sender Game Server Thread not created"));
		}
	}

}

void UNetworkManager::SendDataToLoginServer(const FString& Data) {
	if (SenderLoginServerWorker != nullptr)
	{
		SenderLoginServerWorker->EnqueueDataForSending(Data);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Sender Login Server Worker is null"));
	}
}

void UNetworkManager::SendDataToGameServer(const FString& Data) {
	if (SenderGameServerWorker != nullptr)
	{
		SenderGameServerWorker->EnqueueDataForSending(Data);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Sender Game Server Worker is null"));
	}
}

void UNetworkManager::PollLoginServerNetworkData()
{
	//UE_LOG(LogTemp, Warning, TEXT("Polling for Login Server data..."));
	FString ReceivedData;
	while (ReceiverLoginServerWorker && ReceiverLoginServerWorker->GetData(ReceivedData))
	{
		UE_LOG(LogTemp, Warning, TEXT("Received Login Server data: %s"), *ReceivedData);

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
		UE_LOG(LogTemp, Warning, TEXT("Received Game Server data: %s"), *ReceivedData);

		// Trigger the event or delegate call
		OnGameServerDataReceived.Broadcast(ReceivedData);
	}
}

void UNetworkManager::Shutdown() {

	UE_LOG(LogTemp, Warning, TEXT("Shutting down Network Manager..."));

	// clear the timer
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkLoginServerPollTimerHandle);
		WorldContext->GetTimerManager().ClearTimer(NetworkGameServerPollTimerHandle);
	}


	if (ReceiverLoginServerWorker != nullptr)
	{
		ReceiverLoginServerWorker->Stop();
		if (ReceiverLoginServerThread != nullptr)
		{
			ReceiverLoginServerThread->WaitForCompletion();
			//delete ReceiverLoginServerWorker;
			//ReceiverLoginServerWorker = nullptr;
			//delete ReceiverLoginServerThread;
			//ReceiverLoginServerThread = nullptr;
		}
	}

	if (SenderLoginServerWorker != nullptr)
	{
		SenderLoginServerWorker->Stop();
		if (SenderLoginServerThread != nullptr)
		{
			SenderLoginServerThread->WaitForCompletion();
			//delete SenderLoginServerWorker;
			//SenderLoginServerWorker = nullptr;
			//delete SenderLoginServerThread;
			//SenderLoginServerThread = nullptr;
		}
	}

	if (ReceiverGameServerWorker != nullptr)
	{
		ReceiverGameServerWorker->Stop();
		if (ReceiverGameServerThread != nullptr)
		{
			ReceiverGameServerThread->WaitForCompletion();
			//delete ReceiverGameServerWorker;
			//ReceiverGameServerWorker = nullptr;
			//delete ReceiverGameServerThread;
			//ReceiverGameServerThread = nullptr;
		}
	}

	if (SenderGameServerWorker != nullptr)
	{
		SenderGameServerWorker->Stop();

		if (SenderGameServerThread != nullptr)
		{
			SenderGameServerThread->WaitForCompletion();
			//delete SenderGameServerWorker;
			//SenderGameServerWorker = nullptr;
			//delete SenderGameServerThread;
			//SenderGameServerThread = nullptr;
		}
	}

	if (LoginServerSocket != nullptr)
	{
		LoginServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(LoginServerSocket);
	}

	if (GameServerSocket != nullptr)
	{
		GameServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(GameServerSocket);
	}
}
