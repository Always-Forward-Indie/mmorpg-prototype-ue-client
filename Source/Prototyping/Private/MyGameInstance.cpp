// Fill out your copyright notice in the Description page of Project Settings.


#include "MyGameInstance.h"


UMyGameInstance::UMyGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(LogTemp, Warning, TEXT("GameInstance Constructor called"));

	// Set up the login server IP and port
	LoginServerIP = TEXT("192.168.205.128");
	LoginServerPort = 27014;

	// Set up the game server IP and port
	GameServerIP = TEXT("192.168.205.128");
	GameServerPort = 27015;
}

void UMyGameInstance::Init()
{
    Super::Init();

    InitializeTCPConnection(); // Your custom method to initialize TCP

	// Set up polling timer to poll for new data from the login server
	const float PollIntervalLoginServerData = 0.1f; // How often to poll data, in seconds.
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(NetworkLoginServerPollTimerHandle, this, &UMyGameInstance::PollLoginServerNetworkData, PollIntervalLoginServerData, true);
		UE_LOG(LogTemp, Warning, TEXT("Polling timer for login server data set up successfully."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GetWorld() returned nullptr, polling for login server data timer not set."));
	}


	// Set up polling timer to poll for new data from the game server
	const float PollIntervalGameServerData = 0.01f; // How often to poll data, in seconds.
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(NetworkGameServerPollTimerHandle, this, &UMyGameInstance::PollGameServerNetworkData, PollIntervalGameServerData, true);
		UE_LOG(LogTemp, Warning, TEXT("Polling timer for game server data set up successfully."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GetWorld() returned nullptr, polling timer for game server data not set."));
	}

	// Load the LoginLevel after some delay to prevent issue with the loading screen not showing up as viewport is not yet created
	const float Delay = 0.1f; // Adjust to your needs

	// Schedule the level load after a delay
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(LoadLoginLevelTimerHandle, this, &UMyGameInstance::LoadLoginLevel, Delay, false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GetWorld() returned nullptr, level load delayed."));
		// Consider an alternative approach to delay the level loading
	}
}

// load LoginLevel
void UMyGameInstance::LoadLoginLevel()
{
	LoadLevel(LoginLevelName);
}

void UMyGameInstance::Shutdown()
{
    CleanupTCPConnection();
    Super::Shutdown();
}

void UMyGameInstance::InitializeTCPConnection() {
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

		if(ReceiverLoginServerThread != nullptr)
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

void UMyGameInstance::PollLoginServerNetworkData()
{
	//UE_LOG(LogTemp, Warning, TEXT("Polling for data..."));
	FString ReceivedData;
	while (ReceiverLoginServerWorker && ReceiverLoginServerWorker->GetData(ReceivedData))
	{
		UE_LOG(LogTemp, Warning, TEXT("Received Login Server data: %s"), *ReceivedData);
		// Do something with the data, e.g., process it, send it to other parts of the game, etc.

		ProcessLoginResponse(ReceivedData);
	}
}

void UMyGameInstance::SendLoginServerNetworkData(const FString& Data)
{
	if (SenderLoginServerWorker)
	{
		SenderLoginServerWorker->EnqueueDataForSending(Data);
	}
}

void UMyGameInstance::JoinToLoginServer(const FString& Username, const FString& Password)
{
	//TODO move to separate function check if login and password is valid
	//if Username empty
	if (Username.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Login could not be empty!"));
		ProcessLoginResponse("{\"body\":{\"message\":\"Login could not be empty!\"},\"status\":\"error\"}");

		return;
	}

	//if Password empty
	if (Password.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Password could not be empty!"));
		ProcessLoginResponse("{\"body\":{\"message\":\"Password could not be empty!\"},\"status\":\"error\"}");

		return;
	}

	// Create a JSON object
	TSharedPtr<FJsonObject> JsonObjectLoginData = MakeShareable(new FJsonObject);
	JsonObjectLoginData->SetStringField("type", "authentification");
	JsonObjectLoginData->SetStringField("login", Username);
	JsonObjectLoginData->SetStringField("password", Password);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObjectLoginData.ToSharedRef(), Writer);

	UE_LOG(LogTemp, Warning, TEXT("Sending Login Server data: %s"), *OutputString);

	// Send the data to the server
	SendLoginServerNetworkData(OutputString);
}

void UMyGameInstance::ProcessLoginResponse(const FString& ResponseData)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		// Check if "status" is "success" or "error" if needed
		FString Status = JsonObject->GetStringField("status");

		// Access the nested "body" object
		const TSharedPtr<FJsonObject>* BodyObject;
		if (JsonObject->TryGetObjectField("body", BodyObject))
		{
			FString Message = (*BodyObject)->GetStringField("message");

			if (Status == "success")
			{
				// Set Client ID
				CurrentClientID = (*BodyObject)->GetNumberField("clientId");

				// Set Client Secret
				CurrentClientSecret = (*BodyObject)->GetStringField("hash");

				// Set Client Login
				CurrentClientLogin = (*BodyObject)->GetStringField("login");

				//TODO select real character ID from UI widget according current authentificated client user
				// Set Client Character ID
				if (CurrentClientID == 3) {
					CurrentCharacterID = 1;
				}
				else if (CurrentClientID == 4) {
					CurrentCharacterID = 2;
				}

				// Now broadcast this message
				OnLoginResponseReceived.Broadcast(CurrentClientID, Message);
				

				// Send data to the game server
				JoinToGameServer(CurrentClientSecret, CurrentClientID, CurrentCharacterID);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response"));
	}
}


void UMyGameInstance::PollGameServerNetworkData()
{
	FString ReceivedData;
	while (ReceiverGameServerWorker && ReceiverGameServerWorker->GetData(ReceivedData))
	{
		UE_LOG(LogTemp, Warning, TEXT("Received Game Server data: %s"), *ReceivedData);
		// Do something with the data, e.g., process it, send it to other parts of the game, etc.

		ProcessGameServerResponse(ReceivedData);
	}
}

void UMyGameInstance::SendGameServerNetworkData(const FString& Data)
{
	if (SenderGameServerWorker)
	{
		SenderGameServerWorker->EnqueueDataForSending(Data);
	}
}

// TODO - break packets logic into separate functions
void UMyGameInstance::ProcessGameServerResponse(const FString& ResponseData)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);

	//{"body":{"characterClass":"Mage","characterCurrentHealth":29,"characterCurrentMana":18,"characterExp":57,"characterId":1,"characterLevel":1,"characterName":"TetsMage1Player","characterPosX":1.0,"characterPosY":1.0,"characterPosZ":1.0,"characterRace":"Human"},"header":{"clientId":3,"eventType":"joinGame","hash":"dcb1ee85-9d58-427d-ae58-45a500661fbd","message":"Authentication success for user!","status":"success","timestamp":"2024-01-23 14:44:31","version":"1.0"}}

	// deserialize JSON response
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		// Check if "status" is "success" or "error" if needed
		FString Status;
		FString EventType;
		FString Hash;
		int32 ClientID;
		int32 CharacterID;

		// Access the nested "header" object
		const TSharedPtr<FJsonObject>* HeaderObject;
		if (JsonObject->TryGetObjectField("header", HeaderObject))
		{
			// Set Client ID
			ClientID = (*HeaderObject)->GetNumberField("clientId");

			// Set Client Secret
			Hash = (*HeaderObject)->GetStringField("hash");

			// set status
			Status = (*HeaderObject)->GetStringField("status");

			// set event type
			EventType = (*HeaderObject)->GetStringField("eventType");
		}

		// Access the nested "body" object
		const TSharedPtr<FJsonObject>* BodyObject;
		if (JsonObject->TryGetObjectField("body", BodyObject))
		{
			// Set Client Character ID
			CharacterID = (*BodyObject)->GetNumberField("characterId");
		}

		if (Status == "success" && EventType == "joinGame")
		{
			// add player data to map
			FPlayerData PlayerData;
			PlayerData.ClientID = ClientID;
			PlayerData.CharacterID = CharacterID;
			PlayerData.X = (*BodyObject)->GetNumberField("characterPosX");
			PlayerData.Y = (*BodyObject)->GetNumberField("characterPosY");
			PlayerData.Z = (*BodyObject)->GetNumberField("characterPosZ");
			PlayerData.CharacterName = (*BodyObject)->GetStringField("characterName");
			PlayerData.CharacterClassName = (*BodyObject)->GetStringField("characterClass");
			PlayerData.CharacterRaceName = (*BodyObject)->GetStringField("characterRace");
			PlayerData.CharacterLevel = (*BodyObject)->GetNumberField("characterLevel");
			PlayerData.CharacterExpPoints = (*BodyObject)->GetNumberField("characterExp");
			PlayerData.CharacterCurrentHPPoints = (*BodyObject)->GetNumberField("characterCurrentHealth");
			PlayerData.CharacterCurrentMPPoints = (*BodyObject)->GetNumberField("characterCurrentMana");


			if (CurrentClientID == ClientID) {
				AddPlayerData(CurrentClientID, PlayerData);
				// Load the GameLevel
				LoadLevel(GameLevelName);
				// Get connected players
				GetConnectedPlayers(CurrentClientSecret, CurrentClientID, CurrentCharacterID);
			}
			else {
				AddPlayerData(ClientID, PlayerData);
				SpawnPlayerForClient(ClientID);
			}
		}
		else if (Status == "success" && EventType == "moveCharacter" && LevelBeingLoaded == GameLevelName)
		{
			if (CurrentClientID != ClientID) {
				// update player coordinates
				MovePlayerForClient(ClientID,
					(*BodyObject)->GetNumberField("characterPosX"),
					(*BodyObject)->GetNumberField("characterPosY"),
					(*BodyObject)->GetNumberField("characterPosZ")
				);
			}
		}
		else if (Status == "success" && EventType == "getConnectedCharacters" && LevelBeingLoaded == GameLevelName) {
			// get connected characters list
			TArray<TSharedPtr<FJsonValue>> ConnectedCharacters = (*BodyObject)->GetArrayField("charactersList");

			// iterate over received connected characters list items
			for (int32 i = 0; i < ConnectedCharacters.Num(); i++)
			{
				TSharedPtr<FJsonObject> ConnectedCharacter = ConnectedCharacters[i]->AsObject();

				// add player data to map
				FPlayerData PlayerData;
				PlayerData.ClientID = ConnectedCharacter->GetNumberField("clientId");
				PlayerData.CharacterID = ConnectedCharacter->GetNumberField("characterId");
				PlayerData.X = ConnectedCharacter->GetNumberField("characterPosX");
				PlayerData.Y = ConnectedCharacter->GetNumberField("characterPosY");
				PlayerData.Z = ConnectedCharacter->GetNumberField("characterPosZ");
				PlayerData.CharacterName = ConnectedCharacter->GetStringField("characterName");
				PlayerData.CharacterClassName = ConnectedCharacter->GetStringField("characterClass");
				PlayerData.CharacterRaceName = ConnectedCharacter->GetStringField("characterRace");
				PlayerData.CharacterLevel = ConnectedCharacter->GetNumberField("characterLevel");
				PlayerData.CharacterExpPoints = ConnectedCharacter->GetNumberField("characterExp");
				PlayerData.CharacterCurrentHPPoints = ConnectedCharacter->GetNumberField("characterCurrentHealth");
				PlayerData.CharacterCurrentMPPoints = ConnectedCharacter->GetNumberField("characterCurrentMana");

				// add player data to map
				AddPlayerData(PlayerData.ClientID, PlayerData);

				// spawn player for client
				SpawnPlayerForClient(PlayerData.ClientID);
			}
		}
		else if (Status == "success" && EventType == "disconnection")
		{
			// handle player disconnection
			HandlePlayerDisconnection(ClientID);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response"));
	}
}

void UMyGameInstance::JoinToGameServer(FString& clientSecret, int32& clientID, int32& characterID)
{
	// Create the header JSON object
	TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
	HeaderObject->SetStringField("eventType", "joinGame");
	HeaderObject->SetNumberField("clientId", clientID);
	HeaderObject->SetStringField("hash", clientSecret);

	// Create the body JSON object
	TSharedPtr<FJsonObject> BodyObject = MakeShareable(new FJsonObject);
	BodyObject->SetNumberField("characterId", characterID);

	// Create the main JSON object and add header and body
	TSharedPtr<FJsonObject> MainJsonObject = MakeShareable(new FJsonObject);
	MainJsonObject->SetObjectField("header", HeaderObject);
	MainJsonObject->SetObjectField("body", BodyObject);

	// Serialize the JSON object to a string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);

	UE_LOG(LogTemp, Warning, TEXT("Sending Game Server data: %s"), *OutputString);

	// Send the data to the server
	SendGameServerNetworkData(OutputString);
}

// Get connected players
void UMyGameInstance::GetConnectedPlayers(FString& clientSecret, int32& clientID, int32& characterID)
{
	// Create the header JSON object
	TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
	HeaderObject->SetStringField("eventType", "getConnectedCharacters");
	HeaderObject->SetNumberField("clientId", clientID);
	HeaderObject->SetStringField("hash", clientSecret);

	// Create the body JSON object
	TSharedPtr<FJsonObject> BodyObject = MakeShareable(new FJsonObject);
	BodyObject->SetNumberField("characterId", characterID);

	// Create the main JSON object and add header and body
	TSharedPtr<FJsonObject> MainJsonObject = MakeShareable(new FJsonObject);
	MainJsonObject->SetObjectField("header", HeaderObject);
	MainJsonObject->SetObjectField("body", BodyObject);

	// Serialize the JSON object to a string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);

	UE_LOG(LogTemp, Warning, TEXT("Sending Game Server data: %s"), *OutputString);

	// Send the data to the server
	SendGameServerNetworkData(OutputString);
}

// Send player movement to game server
void UMyGameInstance::SendPlayerMovementToGameServer(FString& clientSecret, int32& clientID, int32& characterID, double& x, double& y, double& z)
{
	// Create the header JSON object
	TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
	HeaderObject->SetStringField("eventType", "moveCharacter");
	HeaderObject->SetNumberField("clientId", clientID);
	HeaderObject->SetStringField("hash", clientSecret);

	// Create the body JSON object
	TSharedPtr<FJsonObject> BodyObject = MakeShareable(new FJsonObject);
	BodyObject->SetNumberField("characterId", characterID);
	BodyObject->SetNumberField("characterPosX", x);
	BodyObject->SetNumberField("characterPosY", y);
	BodyObject->SetNumberField("characterPosZ", z);

	// Create the main JSON object and add header and body
	TSharedPtr<FJsonObject> MainJsonObject = MakeShareable(new FJsonObject);
	MainJsonObject->SetObjectField("header", HeaderObject);
	MainJsonObject->SetObjectField("body", BodyObject);

	// Serialize the JSON object to a string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);

	UE_LOG(LogTemp, Warning, TEXT("Sending Game Server data: %s"), *OutputString);

	// Send the data to the server
	SendGameServerNetworkData(OutputString);
}

void UMyGameInstance::CleanupTCPConnection() {
	UE_LOG(LogTemp, Warning, TEXT("Game Instanse Cleanup..."));

	// Stop the polling timer
	GetWorld()->GetTimerManager().ClearTimer(NetworkLoginServerPollTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(NetworkGameServerPollTimerHandle);


	// Stop the Login Server Receiver networking thread
	if (ReceiverLoginServerWorker != nullptr)
	{
		ReceiverLoginServerWorker->Stop();
		if (ReceiverLoginServerThread != nullptr)
		{
			ReceiverLoginServerThread->WaitForCompletion(); // Ensure the thread has stopped before deletion
			delete ReceiverLoginServerThread;
			ReceiverLoginServerThread = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("Receiver Login Server Thread stopped"));
		}
		delete ReceiverLoginServerWorker;
		ReceiverLoginServerWorker = nullptr;
	}

	// Stop the Login Server Sender networking thread
	if (SenderLoginServerWorker != nullptr)
	{
		SenderLoginServerWorker->Stop();
		if (SenderLoginServerThread != nullptr)
		{
			SenderLoginServerThread->WaitForCompletion(); // Ensure the thread has stopped before deletion
			delete SenderLoginServerThread;
			SenderLoginServerThread = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("Sender Login Server Thread stopped"));
		}
		delete SenderLoginServerWorker;
		SenderLoginServerWorker = nullptr;
	}

	// Close the Login Server socket
	if (LoginServerSocket != nullptr)
	{
		LoginServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(LoginServerSocket);
		LoginServerSocket = nullptr;
		UE_LOG(LogTemp, Warning, TEXT("Login Server Socket closed"));
	}

	// Stop the Game Server Receiver networking thread
	if (ReceiverGameServerWorker != nullptr)
	{
		ReceiverGameServerWorker->Stop();
		if (ReceiverGameServerThread != nullptr)
		{
			ReceiverGameServerThread->WaitForCompletion(); // Ensure the thread has stopped before deletion
			delete ReceiverGameServerThread;
			ReceiverGameServerThread = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("Receiver Game Server Thread stopped"));
		}
		delete ReceiverGameServerWorker;
		ReceiverGameServerWorker = nullptr;
	}

	// Stop the Game Server Sender networking thread
	if (SenderGameServerWorker != nullptr)
	{
		SenderGameServerWorker->Stop();
		if (SenderGameServerThread != nullptr)
		{
			SenderGameServerThread->WaitForCompletion(); // Ensure the thread has stopped before deletion
			delete SenderGameServerThread;
			SenderGameServerThread = nullptr;
			UE_LOG(LogTemp, Warning, TEXT("Sender Game Server Thread stopped"));
		}
		delete SenderGameServerWorker;
		SenderGameServerWorker = nullptr;
	}

	// Close the Game Server socket
	if (GameServerSocket != nullptr)
	{
		GameServerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(GameServerSocket);
		GameServerSocket = nullptr;
		UE_LOG(LogTemp, Warning, TEXT("Game Server Socket closed"));
	}
}

void UMyGameInstance::LoadLevel(const FName& LevelName)
{
	// Store the level name
	LevelBeingLoaded = LevelName;

	UE_LOG(LogTemp, Warning, TEXT("GameInstance address: %p"), this);

	// Show loading screen
	AddLoadingScreen();

	// Asynchronously load the game level
	FLatentActionInfo LatentInfo;
	LatentInfo.CallbackTarget = this;
	LatentInfo.ExecutionFunction = "OnLevelLoaded";
	LatentInfo.Linkage = 0;
	LatentInfo.UUID = 1;

	// Assuming 'this' is a UObject derived class with access to the current world context.
	UWorld* TargetWorld = this->GetWorld();
	if (TargetWorld)
	{
		UGameplayStatics::LoadStreamLevel(TargetWorld, LevelName, true, true, LatentInfo);
	}
}

// This function is called when the level loading is complete
void UMyGameInstance::OnLevelLoaded()
{
	UE_LOG(LogTemp, Warning, TEXT("Level loaded %s"), LevelBeingLoaded);

	// Check if the level being loaded is the LoginLevel
	if (LevelBeingLoaded == LoginLevelName)
	{
		AddLoginWidgetToViewport();
		//show mouse cursor
		GetWorld()->GetFirstPlayerController()->bShowMouseCursor = true;

		// Spawn the custom camera actor and set it as the view target
		LoginLevelCamera = GetWorld()->SpawnActor<AMyCameraActor>(LoginCameraClass, LoginLevelCameraLocation, LoginLevelCameraRotation);
		//check if camera is valid
		if (LoginLevelCamera)
		{
			//set view target with blend using camera reference if player controller is valid
			if (GetWorld()->GetFirstPlayerController())
			{
				GetWorld()->GetFirstPlayerController()->SetViewTargetWithBlend(LoginLevelCamera, 0.5f);

				//play sound
				LoginLevelCamera->PlaySound(LoginMusicSoundSource);
			}
		}
	}
	else
	{
		RemoveLoginWidgetFromViewport();
	}

	// Check if the level being loaded is the GameLevel
	if (LevelBeingLoaded == GameLevelName)
	{
		RemoveLoginWidgetFromViewport();
		GetWorld()->GetFirstPlayerController()->bShowMouseCursor = false;

		// remove the camera from the viewport
		if (LoginLevelCamera)
		{
			LoginLevelCamera->StopSound();
			LoginLevelCamera->Destroy();
		}

		// spawn the player with coordinates
		SpawnPlayerForClient(CurrentClientID);


		// Unload the LoginLevel
		FLatentActionInfo LatentInfo;
		UGameplayStatics::UnloadStreamLevel(this, LoginLevelName, LatentInfo, false);
	}


	// Remove loading screen
	if (LoadingScreenWidget)
	{
		// Schedule the loading screen removal after some delay
		const float Delay = 0.7f; // Adjust to your needs
		GetWorld()->GetTimerManager().SetTimer(RemoveLoadingScreenTimerHandle, this, &UMyGameInstance::RemoveLoadingScreen, Delay, false);
	}
}

void UMyGameInstance::AddLoginWidgetToViewport()
{
	if (LoginScreenWidgetClass)
	{
		LoginScreenWidget = CreateWidget<ULoginWidget>(this, LoginScreenWidgetClass);
		if (LoginScreenWidget)
		{
			LoginScreenWidget->AddToViewport();


			//TODO - Use real data from server

			TArray<FCharacterItemData> Items;
			// add test data to list


			// Create a test character item
			FCharacterItemData Item1;
			Item1.CharacterID = 1;
			Item1.CharacterName = "Test1";

			FCharacterItemData Item2;
			Item2.CharacterID = 2;
			Item2.CharacterName = "Test2";

			// Add the item to the list
			Items.Add(Item1);
			Items.Add(Item2);

			SetCharacterItems(Items);
		}
	}
}

void UMyGameInstance::RemoveLoginWidgetFromViewport()
{
	if (LoginScreenWidget)
	{
		LoginScreenWidget->RemoveFromParent();
		LoginScreenWidget = nullptr; // Clear the reference
	}
}

void UMyGameInstance::AddLoadingScreen()
{
	if (LoadingScreenWidgetClass)
	{
		// Create the widget and add it to the viewport on top of the game UI
		LoadingScreenWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->AddToViewport(999);

			//spawn loading screen actor
			LoadingScreenActor = GetWorld()->SpawnActor<ALoadingSceenActor>(ALoadingSceenActor::StaticClass(), FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f));

			//check if loading screen actor is valid
			if (LoadingScreenActor)
			{
				//play sound
				LoadingScreenActor->PlaySound(LoadingMusicSoundSource);
			}
		}
	}
}

void UMyGameInstance::RemoveLoadingScreen()
{
	if (LoadingScreenWidget)
	{
		// Destroy the loading screen actor
		if (LoadingScreenActor)
		{
			LoadingScreenActor->StopSound();
			LoadingScreenActor->Destroy();
		}

		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr; // Clear the reference
	}
}


FString UMyGameInstance::BoolToString(bool bValue)
{
	return bValue ? TEXT("True") : TEXT("False");
}

void UMyGameInstance::SpawnPlayerForClient(int32 ClientID)
{
	UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForClient %d"), ClientID);
	if (ConnectedPlayers.Contains(ClientID))
	{
		// Check if the player has already been spawned
		if (!SpawnedPlayers.Contains(ClientID))
		{
			FPlayerData& PlayerData = ConnectedPlayers[ClientID];
			FVector SpawnLocation = FVector(PlayerData.X, PlayerData.Y, PlayerData.Z); // Determine spawn location
			FRotator SpawnRotation = FRotator(0.0f, 0.0f, 0.0f); // Determine spawn rotation

			// Spawn the new player
			ABasicPlayer* NewPlayer = GetWorld()->SpawnActor<ABasicPlayer>(MainPlayerClass, SpawnLocation, SpawnRotation);
			if (NewPlayer)
			{
				// Set the player's data
				NewPlayer->SetIsOtherClient(true);
				NewPlayer->SetClientID(PlayerData.ClientID);
				NewPlayer->SetCharacterID(PlayerData.CharacterID);
				NewPlayer->SetCoordinates(PlayerData.X, PlayerData.Y, PlayerData.Z);
				NewPlayer->SetPlayerName(PlayerData.CharacterName);
				NewPlayer->SetPlayerClass(PlayerData.CharacterClassName);
				NewPlayer->SetPlayerRace(PlayerData.CharacterRaceName);
				NewPlayer->SetPlayerLevel(PlayerData.CharacterLevel);
				NewPlayer->SetPlayerExpPoints(PlayerData.CharacterExpPoints);
				NewPlayer->SetPlayerCurrentHPPoints(PlayerData.CharacterCurrentHPPoints);
				NewPlayer->SetPlayerCurrentMPPoints(PlayerData.CharacterCurrentMPPoints);

				// Get the first player controller
				APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

				// If this is current client player
				if (PlayerController && CurrentClientID == ClientID)
				{
					NewPlayer->SetIsOtherClient(false);
					// set additional important secret data
					NewPlayer->SetClientLogin(CurrentClientLogin);
					NewPlayer->SetClientSecret(CurrentClientSecret);
					// Possess the spawned player character with the player controller
					PlayerController->Possess(NewPlayer);
				}

				// Add the player to the SpawnedPlayers map
				SpawnedPlayers.Add(ClientID, NewPlayer);
			}
		}
	}
}

//move client player
void UMyGameInstance::MovePlayerForClient(int32 ClientID, double x, double y, double z)
{
	double MovementThreshold = 5.0f;

	// Check if the player has already been spawned
	if (SpawnedPlayers.Contains(ClientID))
	{
		// Get the player actor
		if (ABasicPlayer* PlayerActor = SpawnedPlayers[ClientID])
		{
			// Get the player controller
			APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

			// If the player controller is valid
			if (PlayerController)
			{
				// Get the player's current location
				FVector CurrentLocation = PlayerActor->GetActorLocation();

				// Calculate the distance between the current location and the new location
				float Distance = FVector::Dist(FVector(x, y, z), CurrentLocation);

				// If the distance is greater than the movement threshold
				if (Distance > MovementThreshold)
				{
					// Move the player to the new location
					PlayerActor->SetCoordinates(x, y, z);
					PlayerActor->SetActorLocation(FVector(x, y, z));
				}
			}
		}
	}
}

void UMyGameInstance::HandlePlayerDisconnection(int32 ClientID)
{
	if (ABasicPlayer** PlayerActor = SpawnedPlayers.Find(ClientID))
	{
		// Optionally: Perform any cleanup needed for the player actor

		// Remove the player actor from the map
		SpawnedPlayers.Remove(ClientID);
	}
}

void UMyGameInstance::AddPlayerData(int32 PlayerID, const FPlayerData& PlayerData)
{
	// Check if the player ID already exists to avoid duplicates
	if (!ConnectedPlayers.Contains(PlayerID))
	{
		ConnectedPlayers.Add(PlayerID, PlayerData);
	}
}

void UMyGameInstance::RemovePlayerData(int32 PlayerID, const FPlayerData& PlayerData)
{
	// Check if the player ID exists
	if (ConnectedPlayers.Contains(PlayerID))
	{
		ConnectedPlayers.Remove(PlayerID);
	}
}

//update payer data coordinates
void UMyGameInstance::UpdatePlayerCoordinates(int32 PlayerID, double x, double y, double z)
{
	if (FPlayerData* PlayerData = ConnectedPlayers.Find(PlayerID))
	{
		PlayerData->X = x;
		PlayerData->Y = y;
		PlayerData->Z = z;
	}
}

// Get character items for client
void UMyGameInstance::GetCharacterItemsData(FString& clientSecret, int32& clientID)
{
	// Create the header JSON object
	TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
	HeaderObject->SetStringField("eventType", "getCharactersList");
	HeaderObject->SetNumberField("clientId", clientID);
	HeaderObject->SetStringField("hash", clientSecret);

	// Create the body JSON object
	TSharedPtr<FJsonObject> BodyObject = MakeShareable(new FJsonObject);
	BodyObject->SetStringField("", "");

	// Create the main JSON object and add header and body
	TSharedPtr<FJsonObject> MainJsonObject = MakeShareable(new FJsonObject);
	MainJsonObject->SetObjectField("header", HeaderObject);
	MainJsonObject->SetObjectField("body", BodyObject);

	// Serialize the JSON object to a string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);

	UE_LOG(LogTemp, Warning, TEXT("Sending Login Server data: %s"), *OutputString);

	// Send the data to the server
	SendLoginServerNetworkData(OutputString);
}

// Set character items to list view widget
void UMyGameInstance::SetCharacterItems(TArray<FCharacterItemData> Items)
{
	UListView* CharacterListView = LoginScreenWidget->GetCharactersListView();

	if (LoginScreenWidget && CharacterListView)
	{
		// Assuming you have a custom entry widget class for characters named UCharacterListItemWidget
		for (const FCharacterItemData& Character : Items)
		{
			UCharacterListItem* CharacterListItemWidget = CreateWidget<UCharacterListItem>(this, CharactersListItemWidgetClass);

			// Check if the widget creation was successful
			if (CharacterListItemWidget)
			{
				UCharacterListItem* CharacterItem = Cast<UCharacterListItem>(CharacterListItemWidget);
				if (CharacterItem)
				{
					// Assuming your UCharacterListItem widget has a function to set character data
					CharacterItem->SetCharacterItemData(Character.CharacterName, Character.CharacterID); // Set character data in your list item widget.
					CharacterListView->AddItem(CharacterItem); // Add the item to the List View.
				}
			}
		}
	}
}