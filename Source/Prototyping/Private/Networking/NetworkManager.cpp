// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonUtilities.h"
#include <Kismet/KismetSystemLibrary.h>

//UNetworkManager::UNetworkManager(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//{
//	UE_LOG(LogTemp, Warning, TEXT("Network Manager Constructor called"));
//
//	// Set up the login server IP and port
//	LoginServerIP = TEXT("127.0.0.1");
//	LoginServerPort = 27014;
//
//	// Set up the game server IP and port
//	GameServerIP = TEXT("127.0.0.1");
//	GameServerPort = 27016;
//}


UNetworkManager::UNetworkManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(LogTemp, Warning, TEXT("Network Manager Constructor called"));

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

	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *ConfigFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Successfully loaded config file from: %s"), *ConfigFilePath);

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			LoginServerIP = JsonObject->GetObjectField("LoginServer")->GetStringField("IP");
			LoginServerPort = JsonObject->GetObjectField("LoginServer")->GetIntegerField("Port");

			GameServerIP = JsonObject->GetObjectField("GameServer")->GetStringField("IP");
			GameServerPort = JsonObject->GetObjectField("GameServer")->GetIntegerField("Port");

			UE_LOG(LogTemp, Warning, TEXT("Login Server: %s:%d"), *LoginServerIP, LoginServerPort);
			UE_LOG(LogTemp, Warning, TEXT("Game Server: %s:%d"), *GameServerIP, GameServerPort);
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

void UNetworkManager::ConnectLoginServer()
{
	// Настраиваем сокет, если он еще не создан
	if (!LoginServerSocket)
	{
		LoginServerSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("LoginServerSocket"), false);
		LoginServerSocket->SetNonBlocking(true);
		LoginServerSocket->SetReuseAddr(true);
		LoginServerSocket->SetRecvErr(true);
	}

	// Устанавливаем IP и порт
	FIPv4Address LoginServerIPAddr;
	FIPv4Address::Parse(LoginServerIP, LoginServerIPAddr);
	FIPv4Endpoint LoginServerEndpoint(LoginServerIPAddr, LoginServerPort);

	// Запускаем подключение
	bIsLoginSocketConnected = LoginServerSocket->Connect(*LoginServerEndpoint.ToInternetAddr());
	LoginConnectionRetryCount = 0;

	// Запускаем таймер для проверки подключения
	FTimerDelegate LoginTimerDelegate;
	LoginTimerDelegate.BindLambda([this, LoginServerEndpoint]()
		{
			if (LoginServerSocket && LoginServerSocket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
			{
				UE_LOG(LogTemp, Warning, TEXT("Login Server socket connected."));
				WorldContext->GetTimerManager().ClearTimer(LoginServerConnectionTimerHandle);

				// Создаем потоки для работы с логин-сервером
				ReceiverLoginServerWorker = new NetworkReceiverWorker(LoginServerSocket);
				ReceiverLoginServerThread = FRunnableThread::Create(ReceiverLoginServerWorker, TEXT("NetworkingLoginServerReceiverThread"));
				SenderLoginServerWorker = new NetworkSenderWorker(LoginServerSocket);
				SenderLoginServerThread = FRunnableThread::Create(SenderLoginServerWorker, TEXT("NetworkingLoginServerSenderThread"));
			}
			else
			{
				LoginConnectionRetryCount++;
				UE_LOG(LogTemp, Warning, TEXT("Waiting for Login Server socket connection... Retry %d"), LoginConnectionRetryCount);
				if (LoginConnectionRetryCount > MaxLoginRetries)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to connect to Login Server after %d retries."), LoginConnectionRetryCount);
					WorldContext->GetTimerManager().ClearTimer(LoginServerConnectionTimerHandle);
					// Показываем попап с выбором: повторить или выйти
					ShowLoginServerConnectionIssuePopup();
				}
			}
		});
	WorldContext->GetTimerManager().SetTimer(LoginServerConnectionTimerHandle, LoginTimerDelegate, 1.0f, true);
}

void UNetworkManager::ConnectGameServer()
{
	// Аналогичная логика для игрового сервера
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
				UE_LOG(LogTemp, Warning, TEXT("Game Server socket connected."));
				WorldContext->GetTimerManager().ClearTimer(GameServerConnectionTimerHandle);

				// Создаем потоки для работы с игровым сервером
				ReceiverGameServerWorker = new NetworkReceiverWorker(GameServerSocket);
				ReceiverGameServerThread = FRunnableThread::Create(ReceiverGameServerWorker, TEXT("NetworkingGameServerReceiverThread"));
				SenderGameServerWorker = new NetworkSenderWorker(GameServerSocket);
				SenderGameServerThread = FRunnableThread::Create(SenderGameServerWorker, TEXT("NetworkingGameServerSenderThread"));
			}
			else
			{
				GameConnectionRetryCount++;
				UE_LOG(LogTemp, Warning, TEXT("Waiting for Game Server socket connection... Retry %d"), GameConnectionRetryCount);
				if (GameConnectionRetryCount > MaxGameRetries)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to connect to Game Server after %d retries."), GameConnectionRetryCount);
					WorldContext->GetTimerManager().ClearTimer(GameServerConnectionTimerHandle);
					// Можно добавить аналогичный попап для игрового сервера, если нужно
					ShowGameServerConnectionIssuePopup();
				}
			}
		});
	WorldContext->GetTimerManager().SetTimer(GameServerConnectionTimerHandle, GameTimerDelegate, 1.0f, true);
}

void UNetworkManager::ShowLoginServerConnectionIssuePopup()
{
	if (MessageBoxPopupClass)
	{
		// Если окно уже создано, не создаём новое
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

			// Устанавливаем окно на экран
			MsgBoxLoginServer->AddToViewport();

			// Подписываемся на события кнопок
			MsgBoxLoginServer->OnLeftButtonClicked.AddDynamic(this, &UNetworkManager::OnLoginServerConnectionRetry);
			MsgBoxLoginServer->OnRightButtonClicked.AddDynamic(this, &UNetworkManager::OnConnectCancel);
		}
	}
}

void UNetworkManager::ShowGameServerConnectionIssuePopup()
{
	if (MessageBoxPopupClass)
	{
		// Если окно уже создано, не создаём новое
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

			// Устанавливаем окно на экран
			MsgBoxGameServer->AddToViewport();

			// Подписываемся на события кнопок
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

	UE_LOG(LogTemp, Warning, TEXT("User chose to retry Login Server connection."));
	// Попытка переподключения: вызываем метод подключения заново
	ConnectLoginServer();
}

void UNetworkManager::OnGameServerConnectionRetry()
{
	UE_LOG(LogTemp, Warning, TEXT("User chose to retry Game Server connection."));
	if (MsgBoxGameServer) 
	{
		MsgBoxGameServer->OnLeftButtonClicked.Clear();
		MsgBoxGameServer->OnRightButtonClicked.Clear();
		MsgBoxGameServer->RemoveFromParent();
		MsgBoxGameServer = nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("User chose to retry Game Server connection."));
	// Попытка переподключения: вызываем метод подключения заново
	ConnectGameServer();
}

void UNetworkManager::OnConnectCancel()
{
	UE_LOG(LogTemp, Warning, TEXT("User cancelled Server connection."));
	// Завершаем игру или выполняем другую логику выхода
	UKismetSystemLibrary::QuitGame(WorldContext, nullptr, EQuitPreference::Quit, true);
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
