// Fill out your copyright notice in the Description page of Project Settings.


#include "MyGameInstance.h"


UMyGameInstance::UMyGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(LogTemp, Warning, TEXT("GameInstance Constructor called"));
}

void UMyGameInstance::Init()
{
    Super::Init();

	// set the network manager
	NetworkManager = NewObject<UNetworkManager>(this);

	// set the ping manager
	PingManager = NewObject<UPingManager>(this);

	// set the authentication manager
	AuthenticationManager = NewObject<UAuthenticationManager>(this);

	// set the player manager
	PlayerManager = NewObject<UPlayerManager>(this);

	// set the MOB manager
	MOBManager = NewObject<UMOBManager>(this);

	// set the spawn zone manager
	SpawnZoneManager = NewObject<USpawnZoneManager>(this);


	UE_LOG(LogTemp, Warning, TEXT("GameInstance Init called"));

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

// init networking setup
void UMyGameInstance::InitNetworkingSetup()
{
	if (!NetworkManager || !AuthenticationManager || !PlayerManager || !PingManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("NetworkManager, AuthenticationManager, PlayerManager or PingManager is nullptr"));
		//return;
	}

	if (NetworkManager != nullptr) {
		// set WorldContext
		NetworkManager->SetWorldContext(GetWorld());
		//Set the message box popup class
		NetworkManager->SetMessageBoxPopupClass(MessageBoxPopupClass);
		// Initialize the network manager connection
		//NetworkManager->InitializeTCPConnection();
		NetworkManager->ConnectLoginServer();
		NetworkManager->ConnectGameServer();
	}

	if (AuthenticationManager != nullptr) {
		// set WorldContext
		AuthenticationManager->SetWorldContext(GetWorld());
		// Initialize the authentication manager
		AuthenticationManager->Initialize(NetworkManager, PingManager);
		// subscribe to the network manager
		AuthenticationManager->SubscribeToNetworkManager();
		// start pinging the server
		AuthenticationManager->StartPing();
	}

	if (PlayerManager != nullptr) {
		// set WorldContext
		PlayerManager->SetWorldContext(GetWorld());
		// Initialize the player manager
		PlayerManager->Initialize(NetworkManager, PingManager);
		// subscribe to the network manager
		PlayerManager->SubscribeToNetworkManager();
		// start pinging the server
		PlayerManager->StartPing();
	}


	if (PingManager != nullptr) {
		// set WorldContext
		PingManager->SetWorldContext(GetWorld());
		// Initialize the ping manager
		PingManager->Initialize(NetworkManager, MonitorStatsWidget);
	}

	if (SpawnZoneManager != nullptr)
	{
		// set WorldContext
		SpawnZoneManager->SetWorldContext(GetWorld());
		// Initialize the spawn zone manager
		SpawnZoneManager->Initialize(NetworkManager);
		// subscribe to the network manager
		SpawnZoneManager->SubscribeToNetworkManager();
	}

	if (MOBManager != nullptr) {
		// set WorldContext
		MOBManager->SetWorldContext(GetWorld());
		// Initialize the MOB manager
		MOBManager->Initialize(NetworkManager);
		// subscribe to the network manager
		MOBManager->SubscribeToNetworkManager();
	}


	if (NetworkManager != nullptr) {
		// Start polling the data from login server
		NetworkManager->StartPollingLoginServer();
		// Start polling the data from game server
		NetworkManager->StartPollingGameServer();
	}
}

// load LoginLevel
void UMyGameInstance::LoadLoginLevel()
{
	if (bDebug) {
		LoadLevel(DebugLevelName);
	}
	else {
		LoadLevel(LoginLevelName);
	}
}

void UMyGameInstance::Shutdown()
{
    Super::Shutdown();

	// send leave game request to Login Server
	if (AuthenticationManager)
	{
		// send leave game request
		AuthenticationManager->SendLeaveGameRequest(ClientData);
	}

	// send leave game request to Game Server
	if (PlayerManager)
	{
		// send leave game request
		PlayerManager->SendLeaveGameRequest(ClientData);
	}

	// Shutdown the network manager
	NetworkManager->Shutdown();
}

// get the network manager
UNetworkManager* UMyGameInstance::GetNetworkManager()
{
	return NetworkManager;
}

// get authentication manager
UAuthenticationManager* UMyGameInstance::GetAuthenticationManager()
{
	return AuthenticationManager;
}

// get current client data
FClientDataStruct UMyGameInstance::GetCurrentClientData()
{
	return ClientData;
}

//get player manager
UPlayerManager* UMyGameInstance::GetPlayerManager()
{
	return PlayerManager;
}

UMOBManager* UMyGameInstance::GetMOBManager()
{
	return MOBManager;
}

TSubclassOf<class ABasicMOB> UMyGameInstance::GetBasicMOBClass()
{
	return BasicMOBClass;
}

USpawnZoneManager* UMyGameInstance::GetSpawnZoneManager()
{
	return SpawnZoneManager;
}

TSubclassOf<class AMobSpawnZone> UMyGameInstance::GetBasicSpawnZoneClass()
{
	return BasicSpawnZoneClass;
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


	UWorld* TargetWorld = GetWorld();
	if (TargetWorld)
	{
		UGameplayStatics::LoadStreamLevel(TargetWorld, LevelName, true, true, LatentInfo);
	}
}

// This function is called when the level loading is complete
void UMyGameInstance::OnLevelLoaded()
{
	// level loaded
	UE_LOG(LogTemp, Warning, TEXT("Level loaded %s"), *LevelBeingLoaded.ToString());
	

	// Check if the level being loaded is the LoginLevel
	if (LevelBeingLoaded == LoginLevelName)
	{
		// Add login widget to viewport
		AddLoginWidgetToViewport();

		// Add monitor stats widget to viewport
		AddMonitorStatsWidgetToViewport();

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

		// Initialize networking setup
		InitNetworkingSetup();
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
	}

	if (LevelBeingLoaded == DebugLevelName)
	{
		RemoveLoginWidgetFromViewport();
		GetWorld()->GetFirstPlayerController()->bShowMouseCursor = false;


		FClientDataStruct clientData;
		clientData.clientId = 1;
		clientData.characterData.characterId = 1;
		clientData.characterData.characterPosition.positionX = 0.0f;
		clientData.characterData.characterPosition.positionY = 0.0f;
		clientData.characterData.characterPosition.positionZ = 90.0f;
		CurrentCharacterID = 1;
		CurrentClientID = 1;

		ClientData = clientData;



		AddPlayerData(clientData.clientId, clientData);
		SpawnPlayerForClient(clientData.clientId);
	}

	UWorld* TargetWorld = GetWorld();
	if (TargetWorld && LevelBeingLoaded != LoginLevelName)
	{
		// Unload the prev level
		FLatentActionInfo LatentInfo;
		LatentInfo.CallbackTarget = this;
		LatentInfo.Linkage = 0;
		LatentInfo.ExecutionFunction = "OnLevelUnloaded";
		LatentInfo.UUID = 2; // Any unique ID

		UGameplayStatics::UnloadStreamLevel(TargetWorld, LoginLevelName, LatentInfo, false);
	}


	// Remove loading screen
	if (LoadingScreenWidget)
	{
		// Schedule the loading screen removal after some delay
		const float Delay = 0.7f; // Adjust to your needs
		GetWorld()->GetTimerManager().SetTimer(RemoveLoadingScreenTimerHandle, this, &UMyGameInstance::RemoveLoadingScreen, Delay, false);
	}
}

void UMyGameInstance::OnLevelUnloaded()
{
	UE_LOG(LogTemp, Warning, TEXT("Level unloaded %s"), *LevelBeingLoaded.ToString());

}

// add monitor to viewport
void UMyGameInstance::AddMonitorStatsWidgetToViewport()
{
	if (MonitorStatsWidgetClass)
	{
		MonitorStatsWidget = CreateWidget<UMonitorStatsWidget>(this, MonitorStatsWidgetClass);
		if (MonitorStatsWidget)
		{
			MonitorStatsWidget->AddToViewport();
		}
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

void UMyGameInstance::SpawnPlayerForClient(int32 ClientID)
{
	if (ConnectedPlayers.Contains(ClientID))
	{
		// Check if the player has already been spawned
		if (!SpawnedPlayers.Contains(ClientID))
		{
			FClientDataStruct PlayerData = ConnectedPlayers[ClientID];

			FVector SpawnLocation = FVector(PlayerData.characterData.characterPosition.positionX, 
				PlayerData.characterData.characterPosition.positionY, 
				PlayerData.characterData.characterPosition.positionZ); // Determine spawn location
			FRotator SpawnRotation = FRotator(0.0f, PlayerData.characterData.characterPosition.rotationZ, 0.0f); // Determine spawn rotation

			// Spawn the new player
			ABasicPlayer* NewPlayer = GetWorld()->SpawnActor<ABasicPlayer>(MainPlayerClass, SpawnLocation, SpawnRotation);
			if (NewPlayer)
			{
				UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForClient with pos %d %f %f %f rot %f"), ClientID, SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z, SpawnRotation.Yaw);

				// Set the player's data
				NewPlayer->SetIsOtherClient(true);
				NewPlayer->SetClientID(PlayerData.clientId);
				NewPlayer->SetCharacterID(PlayerData.characterData.characterId);
				NewPlayer->SetCoordinates(PlayerData.characterData.characterPosition.positionX, 
					PlayerData.characterData.characterPosition.positionY,
					PlayerData.characterData.characterPosition.positionZ,
					PlayerData.characterData.characterPosition.rotationZ
				);
				NewPlayer->SetPlayerName(PlayerData.characterData.characterName);
				NewPlayer->SetPlayerClass(PlayerData.characterData.characterClass);
				NewPlayer->SetPlayerRace(PlayerData.characterData.characterRace);
				NewPlayer->SetPlayerLevel(PlayerData.characterData.characterLevel);
				NewPlayer->SetPlayerExpPoints(PlayerData.characterData.characterExperiencePoints);
				NewPlayer->SetPlayerCurrentHPPoints(PlayerData.characterData.characterCurrentHealth);
				NewPlayer->SetPlayerCurrentMPPoints(PlayerData.characterData.characterCurrentMana);

				// Get the first player controller
				APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

				// If this is current client player
				if (PlayerController && GetCurrentClientID() == ClientID)
				{
					UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForCurrentClient - Recieved ID: %d, Found ID: %d"), ClientID, PlayerData.clientId);

					NewPlayer->SetIsOtherClient(false);
					// set additional important secret data
					NewPlayer->SetClientLogin(PlayerData.clientLogin);
					NewPlayer->SetClientSecret(PlayerData.hash);
					// Possess the spawned player character with the player controller
					PlayerController->Possess(NewPlayer);
				}

				// Add the player to the SpawnedPlayers map
				SpawnedPlayers.Add(ClientID, NewPlayer);
			}
		}
	}
}

//TODO - work on movement logic for client player
//move client player
void UMyGameInstance::MovePlayerForClient(const int32 ClientID, const FClientDataStruct& clientData, const FMessageDataStruct& MessageData)
{
	// Check if the player has already been spawned
	if (SpawnedPlayers.Contains(ClientID))
	{
		// Get the player actor
		if (ABasicPlayer* PlayerActor = SpawnedPlayers[ClientID])
		{
				// If the distance is greater than the movement threshold
				if (PlayerActor->GetIsOtherClient())
				{
					// Set Message Data
					PlayerActor->SetMessageData(MessageData);

					// Move the player to the new location
					PlayerActor->SetCoordinates(clientData.characterData.characterPosition.positionX,
						clientData.characterData.characterPosition.positionY,
						clientData.characterData.characterPosition.positionZ,
						clientData.characterData.characterPosition.rotationZ);
				}
		}
	}
}

void UMyGameInstance::HandlePlayerDisconnection(int32 ClientID)
{
	if (ABasicPlayer** PlayerActor = SpawnedPlayers.Find(ClientID))
	{
		// Destroy the player actor
		(*PlayerActor)->Destroy();

		// Remove the player actor from the map
		SpawnedPlayers.Remove(ClientID);
		RemovePlayerData(ClientID);
	}
}

void UMyGameInstance::AddPlayerData(int32 ClientID, const FClientDataStruct clientData)
{
	// Check if the player ID already exists to avoid duplicates
	if (!ConnectedPlayers.Contains(ClientID))
	{
		UE_LOG(LogTemp, Warning, TEXT("AddPlayerData %d"), clientData.clientId);
		ConnectedPlayers.Add(ClientID, clientData);
	}
}

void UMyGameInstance::RemovePlayerData(int32 ClientID)
{
	// Check if the player ID exists
	if (ConnectedPlayers.Contains(ClientID))
	{
		ConnectedPlayers.Remove(ClientID);
	}
}

//update payer data coordinates
void UMyGameInstance::UpdatePlayerCoordinates(int32 PlayerID, double x, double y, double z, double rotZ)
{
	if (FClientDataStruct* PlayerData = ConnectedPlayers.Find(PlayerID))
	{
		PlayerData->characterData.characterPosition.positionX = x;
		PlayerData->characterData.characterPosition.positionY = y;
		PlayerData->characterData.characterPosition.positionZ = z;
		PlayerData->characterData.characterPosition.rotationZ = rotZ;
	}
}

// Set character items to list view widget
void UMyGameInstance::SetCharacterItems(TArray<FCharacterDataStruct> Items)
{
	UListView* CharacterListView = LoginScreenWidget->GetCharactersListView();

	if (LoginScreenWidget && CharacterListView)
	{
		// Assuming you have a custom entry widget class for characters named UCharacterListItemWidget
		for (const FCharacterDataStruct& Character : Items)
		{
			UCharacterListItem* CharacterListItemWidget = CreateWidget<UCharacterListItem>(this, CharactersListItemWidgetClass);

			// Check if the widget creation was successful
			if (CharacterListItemWidget)
			{
				UCharacterListItem* CharacterItem = Cast<UCharacterListItem>(CharacterListItemWidget);
				if (CharacterItem)
				{
					// Assuming your UCharacterListItem widget has a function to set character data
					CharacterItem->SetCharacterItemData(Character.characterName, Character.characterId); // Set character data in your list item widget.
					CharacterListView->AddItem(CharacterItem); // Add the item to the List View.
				}
			}
		}
	}
}

// set character id
void UMyGameInstance::SetCurrentCharacterID(int32 CharacterID)
{
	ClientData.characterData.characterId = CharacterID;
	UE_LOG(LogTemp, Warning, TEXT("SetCurrentCharacterID %d"), CharacterID);
}

// get character id
int32 UMyGameInstance::GetCurrentCharacterID()
{
	return ClientData.characterData.characterId;
}

// set current client id
void UMyGameInstance::SetCurrentClientID(int32 ClientID)
{
	ClientData.clientId = ClientID;
}

// get current client id
int32 UMyGameInstance::GetCurrentClientID()
{
	return ClientData.clientId;
}

// set current client secret
void UMyGameInstance::SetCurrentClientHash(FString ClientSecret)
{
	ClientData.hash = ClientSecret;
}

// get current client secret
FString UMyGameInstance::GetCurrentClientHash()
{
	return ClientData.hash;
}