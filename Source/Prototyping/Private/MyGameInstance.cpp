// Fill out your copyright notice in the Description page of Project Settings.


#include "MyGameInstance.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Combat/DamageEffectHandler.h"
#include "Gameplay/Combat/HealingEffectHandler.h"
#include "Gameplay/Combat/BuffEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/ExperienceNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Gameplay/Skills/PlayerSkillSystemFactory.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "Services/TimeSyncService.h"
#include "Networking/PingManager.h"
#include "UI/UIManager.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/NPCs/NPCNetworkHandler.h"

// Forward declarations for manager types
class UDialogueManager;
class UQuestManager;
class UEquipmentManager;
class UVendorManager;
class URepairManager;
class UTradeManager;
class UBestiaryNetworkHandler;
class UChatManager;

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
    
	// set the item manager
	ItemManager = NewObject<UItemManager>(this);

	// set the inventory manager
	InventoryManager = NewObject<UInventoryManager>(this);

	// set the harvest manager
	HarvestManager = NewObject<UHarvestManager>(this);

	// set the experience manager
	ExperienceManager = NewObject<UExperienceManager>(this);

	// set the experience network handler
	ExperienceNetworkHandler = NewObject<UExperienceNetworkHandler>(this);

	// Initialize new combat system
	CombatSystemManager = NewObject<UCombatSystemManager>(this);
	SkillSystemManager = NewObject<USkillSystemManager>(this);
	CombatNetworkHandler = NewObject<UCombatNetworkHandler>(this);

	// Initialize NPC System
	NPCManager = NewObject<UNPCManager>(this);
	NPCNetworkHandler = NewObject<UNPCNetworkHandler>(this);

	// Initialize player stats manager
	PlayerStatsManager = NewObject<UPlayerStatsManager>(this);

	// Initialize time sync service
	TimeSyncService = NewObject<UTimeSyncService>(this);
	if (TimeSyncService)
	{
		TimeSyncService->SetWorldContext(GetWorld());
		TimeSyncService->Initialize();
		UE_LOG(LogTemp, Warning, TEXT("TimeSyncService initialized with world context"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create TimeSyncService"));
	}

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

	// Start Ping for servers
	NetworkManager->OnGameServerSocketConnected.AddDynamic(this, &UMyGameInstance::StartPingGameServer);
	NetworkManager->OnLoginServerSocketConnected.AddDynamic(this, &UMyGameInstance::StartPingLoginServer);

	InitGameSystems();
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
		NetworkManager->ConnectChunkServer();
	}

	if (AuthenticationManager != nullptr) {
		// set WorldContext
		AuthenticationManager->SetWorldContext(GetWorld());
		// Initialize the authentication manager
		AuthenticationManager->Initialize(NetworkManager, PingManager);
		// subscribe to the network manager
		AuthenticationManager->SubscribeToNetworkManager();
	}

	if (PlayerManager != nullptr) {
		// set WorldContext
		PlayerManager->SetWorldContext(GetWorld());
		// Initialize the player manager
		PlayerManager->Initialize(NetworkManager, PingManager);
		// subscribe to the network manager
		PlayerManager->SubscribeToNetworkManager();
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
    
	if (ItemManager != nullptr) {
		// set WorldContext
		ItemManager->SetWorldContext(GetWorld());
		// set game instance
		ItemManager->SetGameInstance(this);
		// Initialize the item manager
		ItemManager->Initialize(NetworkManager);
		// subscribe to the network manager
		ItemManager->SubscribeToNetworkManager();
	}

	if (InventoryManager != nullptr) {
		// set WorldContext
		InventoryManager->SetWorldContext(GetWorld());
		// set game instance
		InventoryManager->SetGameInstance(this);
		// Initialize the inventory manager
		InventoryManager->Initialize(NetworkManager);
		// subscribe to the network manager
		InventoryManager->SubscribeToNetworkManager();
	}

	if (HarvestManager != nullptr) {
		// set WorldContext
		HarvestManager->SetWorldContext(GetWorld());
		// set game instance
		HarvestManager->SetGameInstance(this);
		// Initialize the harvest manager
		HarvestManager->Initialize(NetworkManager);
		// subscribe to the network manager
		HarvestManager->SubscribeToNetworkManager();
	}

	// Initialize experience system
	if (ExperienceManager != nullptr) {
		// Initialize the experience manager
		ExperienceManager->Initialize(this, NetworkManager);
		UE_LOG(LogTemp, Warning, TEXT("ExperienceManager initialized"));
	}

	if (ExperienceNetworkHandler != nullptr && ExperienceManager != nullptr) {
		// Initialize the experience network handler
		ExperienceNetworkHandler->Initialize(ExperienceManager, this, NetworkManager);
		// Subscribe to network events
		ExperienceNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("ExperienceNetworkHandler initialized and subscribed"));
	}

	// Initialize new combat system
	if (CombatSystemManager != nullptr) {
		// set WorldContext
		CombatSystemManager->SetWorldContext(GetWorld());
		// Initialize the combat system manager
		CombatSystemManager->Initialize(this, NetworkManager);
		
		// Register default effect handlers using proper UINTERFACE method
		UDamageEffectHandler* DamageHandler = NewObject<UDamageEffectHandler>(this);
		TScriptInterface<ISkillEffectHandler> DamageInterface;
		DamageInterface.SetObject(DamageHandler);
		DamageInterface.SetInterface(Cast<ISkillEffectHandler>(DamageHandler));
		CombatSystemManager->RegisterEffectHandler(DamageInterface);

		UHealingEffectHandler* HealingHandler = NewObject<UHealingEffectHandler>(this);
		TScriptInterface<ISkillEffectHandler> HealingInterface;
		HealingInterface.SetObject(HealingHandler);
		HealingInterface.SetInterface(Cast<ISkillEffectHandler>(HealingHandler));
		CombatSystemManager->RegisterEffectHandler(HealingInterface);

		UBuffEffectHandler* BuffHandler = NewObject<UBuffEffectHandler>(this);
		TScriptInterface<ISkillEffectHandler> BuffInterface;
		BuffInterface.SetObject(BuffHandler);
		BuffInterface.SetInterface(Cast<ISkillEffectHandler>(BuffHandler));
		CombatSystemManager->RegisterEffectHandler(BuffInterface);
		
		UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager initialized with UINTERFACE effect handlers"));
	}

	if (SkillSystemManager != nullptr) {
		// Initialize the skill system manager
		SkillSystemManager->Initialize(CombatSystemManager, NetworkManager);
		UE_LOG(LogTemp, Warning, TEXT("SkillSystemManager initialized"));
	}

	// Initialize player skill system using factory pattern
	PlayerSkillSystemFactory = NewObject<UPlayerSkillSystemFactory>(this);
	if (PlayerSkillSystemFactory && SkillSystemManager && NetworkManager)
	{
		bool bSkillSystemCreated = PlayerSkillSystemFactory->CreateCompletePlayerSkillSystem(
			SkillSystemManager,
			NetworkManager,
			SkillDefinitionsDataTable,
			TimeSyncService,
			this
		);

		if (bSkillSystemCreated)
		{
			// Get references to created components
			PlayerSkillManager = PlayerSkillSystemFactory->GetPlayerSkillManager();
			SkillDefinitionRepository = PlayerSkillSystemFactory->GetSkillDefinitionRepository();
			PlayerSkillNetworkHandler = PlayerSkillSystemFactory->GetPlayerSkillNetworkHandler();

			UE_LOG(LogTemp, Warning, TEXT("Player skill system created successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create player skill system"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot create player skill system - missing dependencies or SkillDefinitionsDataTable not configured"));
	}

	// Initialize CombatNetworkHandler centrally AFTER CombatSystemManager
	if (CombatNetworkHandler != nullptr && CombatSystemManager != nullptr) {
		// Initialize the combat network handler
		CombatNetworkHandler->Initialize(CombatSystemManager, NetworkManager);
		// Subscribe to network events
		CombatNetworkHandler->SubscribeToNetworkManager();
		UE_LOG(LogTemp, Warning, TEXT("CombatNetworkHandler initialized centrally"));
	}

	// Initialize NPC Manager
	if (NPCManager)
	{
		NPCManager->SetWorldContext(GetWorld());
		NPCManager->SetGameInstance(this);
		NPCManager->Initialize(GetNetworkManager());
		NPCManager->SubscribeToNetworkManager();
		
		// Set NPCDefinitionTable if available
		if (NPCDefinitionTable)
		{
			NPCManager->SetNPCDefinitionTable(NPCDefinitionTable);
			UE_LOG(LogTemp, Warning, TEXT("NPCManager: NPCDefinitionTable assigned from GameInstance"));
		}
		
		UE_LOG(LogTemp, Warning, TEXT("NPCManager initialized"));
	}

	// Initialize NPC Network Handler
	if (NPCNetworkHandler && NPCManager)
	{
		NPCNetworkHandler->Initialize(NPCManager, GetNetworkManager());
		NPCNetworkHandler->SubscribeToNetworkEvents();
		
		UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler initialized"));
	}

	if (NetworkManager != nullptr) {
		// Start polling the data from login server
		NetworkManager->StartPollingLoginServer();
		// Start polling the data from game server
		NetworkManager->StartPollingGameServer();
		// Start polling the data from chunk server
		NetworkManager->StartPollingChunkServer();
	}
}

void UMyGameInstance::StartPingGameServer()
{
	if (PlayerManager != nullptr) 
	{
		// Start pinging the game server
		PlayerManager->StartPing();
	}
}

void UMyGameInstance::StartPingLoginServer()
{
	if (AuthenticationManager != nullptr)
	{
		// Start pinging the login server
		AuthenticationManager->StartPing();
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

TSubclassOf<class ABasicNPC> UMyGameInstance::GetBasicNPCClass()
{
	return BasicNPCClass;
}


USpawnZoneManager* UMyGameInstance::GetSpawnZoneManager()
{
	return SpawnZoneManager;
}

TSubclassOf<class AMobSpawnZone> UMyGameInstance::GetBasicSpawnZoneClass()
{
	return BasicSpawnZoneClass;
}

UItemManager* UMyGameInstance::GetItemManager()
{
	return ItemManager;
}

UInventoryManager* UMyGameInstance::GetInventoryManager()
{
	return InventoryManager;
}

UHarvestManager* UMyGameInstance::GetHarvestManager()
{
	return HarvestManager;
}

UExperienceManager* UMyGameInstance::GetExperienceManager()
{
	return ExperienceManager;
}

UExperienceNetworkHandler* UMyGameInstance::GetExperienceNetworkHandler()
{
	return ExperienceNetworkHandler;
}

UCombatSystemManager* UMyGameInstance::GetCombatSystemManager()
{
	return CombatSystemManager;
}

USkillSystemManager* UMyGameInstance::GetSkillSystemManager()
{
	return SkillSystemManager;
}

UCombatNetworkHandler* UMyGameInstance::GetCombatNetworkHandler()
{
	return CombatNetworkHandler;
}

UPlayerSkillManager* UMyGameInstance::GetPlayerSkillManager()
{
	return PlayerSkillManager;
}

USkillDefinitionRepository* UMyGameInstance::GetSkillDefinitionRepository()
{
	return SkillDefinitionRepository;
}

UPlayerSkillNetworkHandler* UMyGameInstance::GetPlayerSkillNetworkHandler()
{
	return PlayerSkillNetworkHandler;
}

UPlayerSkillSystemFactory* UMyGameInstance::GetPlayerSkillSystemFactory()
{
	return PlayerSkillSystemFactory;
}

TSubclassOf<class ADroppedItemActor> UMyGameInstance::GetDroppedItemActorClass()
{
	return DroppedItemActorClass;
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
	UE_LOG(LogTemp, Warning, TEXT("Level unloaded trigerred and loaded new level: %s"), *LevelBeingLoaded.ToString());

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
		if (!LoadingScreenWidget)
		{
			// Create the widget and add it to the viewport on top of the game UI
			LoadingScreenWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
		}
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
				//set next level exp
				NewPlayer->SetPlayerNextLevelExp(PlayerData.characterData.characterExpForLevelEnd);

				//set exp points
				NewPlayer->SetPlayerExpPoints(PlayerData.characterData.characterExperiencePoints);
				NewPlayer->SetPlayerCurrentHPPoints(PlayerData.characterData.characterCurrentHealth);
				NewPlayer->SetPlayerCurrentMPPoints(PlayerData.characterData.characterCurrentMana);
				//set attributes
				NewPlayer->SetPlayerAttributes(PlayerData.characterData.characterAttributes.attributesData);
				//set tag
				NewPlayer->SetPlayerTag(*FString::FromInt(PlayerData.characterData.characterId));
				NewPlayer->SetPlayerTag("Player");


				// Register with combat system now that player has valid data
				if (CombatSystemManager && PlayerData.characterData.characterId > 0)
				{
					// Убедимся что объект валиден перед регистрацией
					if (IsValid(NewPlayer) && !NewPlayer->IsActorBeingDestroyed())
					{
						TScriptInterface<ICombatable> CombatableInterface;
						CombatableInterface.SetObject(NewPlayer);
						CombatableInterface.SetInterface(Cast<ICombatable>(NewPlayer));
						
						CombatSystemManager->RegisterCombatable(CombatableInterface);
						UE_LOG(LogTemp, Warning, TEXT("Player %d registered with combat system"), PlayerData.characterData.characterId);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("Player %d is invalid or being destroyed, not registering"), PlayerData.characterData.characterId);
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Cannot register player with combat system - invalid data or CombatSystemManager null"));
				}

				// Get the first player controller
				APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

				// Add to spawned players map
				SpawnedPlayers.Add(ClientID, NewPlayer);

				// If this is the current client, possession happens in the player's begin play
				if (ClientID == CurrentClientID)
				{
					// Set the character as the current player's character
					Player = NewPlayer;
					Player->SetIsOtherClient(false);
                    
                    // Set additional important secret data
                    NewPlayer->SetClientLogin(PlayerData.clientLogin);
                    NewPlayer->SetClientSecret(PlayerData.hash);
                    
                    // Possess the player and create the HUD
                    PlayerController->Possess(Player);
                    //NewPlayer->CreateHUD();

					// Request inventory data for the current player
					if (InventoryManager)
					{
						InventoryManager->RequestInventoryData(PlayerData.characterData.characterId);
						UE_LOG(LogTemp, Warning, TEXT("Requested inventory data for character ID: %d"), PlayerData.characterData.characterId);
					}
				}
			}
		}
	}
}

void UMyGameInstance::SetCurrentClientID(int32 ClientID)
{
	CurrentClientID = ClientID;

	// set client ID to client data
	ClientData.clientId = ClientID;
}

int32 UMyGameInstance::GetCurrentClientID()
{
	return CurrentClientID;
}

void UMyGameInstance::SetCurrentClientHash(FString ClientSecret)
{
	CurrentClientSecret = ClientSecret;

	// set client token to client data
	ClientData.hash = ClientSecret;
}

FString UMyGameInstance::GetCurrentClientHash()
{
	return CurrentClientSecret;
}

void UMyGameInstance::SetCurrentCharacterID(int32 CharacterID)
{
	CurrentCharacterID = CharacterID;
    
    // Update the character ID in the client data
    ClientData.characterData.characterId = CharacterID;
    
    UE_LOG(LogTemp, Warning, TEXT("SetCurrentCharacterID %d"), CharacterID);
}

int32 UMyGameInstance::GetCurrentCharacterID()
{
	return CurrentCharacterID;
}

void UMyGameInstance::JoinSelectedCharacterToGame()
{
    UE_LOG(LogTemp, Warning, TEXT("JoinSelectedCharacterToGame called with CharacterID: %d"), CurrentCharacterID);
    
    if (CurrentCharacterID <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot join game: No character selected."));
        return;
    }
    
    if (CurrentClientID <= 0 || CurrentClientSecret.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot join game: Client not authenticated."));
        return;
    }
    
    // Send the join game request
    if (PlayerManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("Sending join game request for character %d"), CurrentCharacterID);
        PlayerManager->SendJoinCharacterChunkRequest(ClientData);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot join game: PlayerManager is null."));
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

		// If the player was spawned, remove it from the map and destroy it
		if (SpawnedPlayers.Contains(ClientID))
		{
			ABasicPlayer* PlayerToRemove = SpawnedPlayers[ClientID];
			if (PlayerToRemove)
			{
				PlayerToRemove->Destroy();
			}
			SpawnedPlayers.Remove(ClientID);
		}
	}
}

void UMyGameInstance::MovePlayerForClient(const int32 ClientID, const FClientDataStruct& clientData, const FMessageDataStruct& MessageData)
{
	// If the player has been spawned and it's not the current client
	if (SpawnedPlayers.Contains(ClientID) && ClientID != CurrentClientID)
	{
		ABasicPlayer* PlayerToMove = SpawnedPlayers[ClientID];
		if (PlayerToMove)
		{
			FString CurrentTimestamp = MessageData.timestamp;

			//set character coordinates
			PlayerToMove->SetCoordinates(clientData.characterData.characterPosition.positionX,
				clientData.characterData.characterPosition.positionY,
				clientData.characterData.characterPosition.positionZ,
				clientData.characterData.characterPosition.rotationZ);
		}
	}
}

void UMyGameInstance::HandlePlayerDisconnection(int32 ClientID)
{
	// Remove the player from our map and destroy the actor if it exists
	RemovePlayerData(ClientID);
}

void UMyGameInstance::UpdatePlayerCoordinates(int32 PlayerID, double x, double y, double z, double rotZ)
{
	// Find the player in the map
	if (ConnectedPlayers.Contains(PlayerID))
	{
		FClientDataStruct& PlayerData = ConnectedPlayers[PlayerID];

		// Update the player's position in our map
		PlayerData.characterData.characterPosition.positionX = x;
		PlayerData.characterData.characterPosition.positionY = y;
		PlayerData.characterData.characterPosition.positionZ = z;
		PlayerData.characterData.characterPosition.rotationZ = rotZ;

		// If the player has been spawned, update its position in the world
		if (SpawnedPlayers.Contains(PlayerID))
		{
			ABasicPlayer* PlayerToMove = SpawnedPlayers[PlayerID];
			if (PlayerToMove)
			{
				FVector NewLocation(x, y, z);
				FRotator NewRotation(0.0f, rotZ, 0.0f);

				PlayerToMove->SetActorLocationAndRotation(NewLocation, NewRotation);
			}
		}
	}
}

// Set character items to list view widget
void UMyGameInstance::SetCharacterItems(TArray<FCharacterDataStruct> Items)
{
	UListView* CharacterListView = LoginScreenWidget->GetCharactersListView();

	if (LoginScreenWidget && CharacterListView)
	{
		// Clear existing items first
		CharacterListView->ClearListItems();
		
		// Populate the list with character items
		for (const FCharacterDataStruct& Character : Items)
		{
			UCharacterListItem* CharacterListItemWidget = CreateWidget<UCharacterListItem>(this, CharactersListItemWidgetClass);

			// Check if the widget creation was successful
			if (CharacterListItemWidget)
			{
				// Set character data in the list item widget
				CharacterListItemWidget->SetCharacterItemData(Character.characterName, Character.characterId);
				
				// Add the item to the List View
				CharacterListView->AddItem(CharacterListItemWidget);
				
				UE_LOG(LogTemp, Warning, TEXT("Added character to list: %s (ID: %d)"), 
					*Character.characterName, Character.characterId);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to set character items - LoginScreenWidget or CharacterListView is null"));
	}
}

// Combat system functions
ABasicPlayer* UMyGameInstance::GetPlayerByCharacterId(int32 CharacterId)
{
	// Check if we have a local player with this character ID
	if (Player && Player->GetPlayerCharacterID() == CharacterId)
	{
		return Player;
	}

	// Check spawned players for the character ID
	for (auto& PlayerPair : SpawnedPlayers)
	{
		ABasicPlayer* PlayerActor = PlayerPair.Value;
		if (PlayerActor && IsValid(PlayerActor) && PlayerActor->GetPlayerCharacterID() == CharacterId)
		{
			return PlayerActor;
		}
	}

	// If not found, return nullptr
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Player with character ID %d not found"), CharacterId);
	return nullptr;
}

void UMyGameInstance::PlayCombatAnimation(const FCombatAnimationData& AnimationData)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Playing combat animation %s for character ID %d"),
		*AnimationData.AnimationName, AnimationData.CharacterId);

	// Check if the character is the player
	ABasicPlayer* SourcePlayer = GetPlayerByCharacterId(AnimationData.CharacterId);
	
	if (SourcePlayer)
	{
		// Play animation on the player character
		UE_LOG(LogTemp, Warning, TEXT("Playing animation on player character: %s"), *AnimationData.AnimationName);
		// TODO: Implement animation playing in BasicPlayer class
		// SourcePlayer->PlayAnimation(AnimationData.AnimationName);
	}
	else
	{
		// Check if it's a MOB
		// Convert the character ID to a string to match how MOB UIDs are stored
		FString MobUid = FString::FromInt(AnimationData.CharacterId);
		
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(*MobUid), FoundActors);
		
		if (FoundActors.Num() > 0)
		{
			ABasicMOB* MOB = Cast<ABasicMOB>(FoundActors[0]);
			if (MOB)
			{
				// Play animation on the MOB
				UE_LOG(LogTemp, Warning, TEXT("Playing animation on MOB %s: %s"), 
					*MOB->GetMobName(), *AnimationData.AnimationName);
				// TODO: Implement animation playing in BasicMOB class
				// MOB->PlayAnimation(AnimationData.AnimationName);
			}
		}
	}
}

void UMyGameInstance::ProcessCombatAction(const FCombatActionData& ActionData)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Processing combat action %s from caster %d to target %d"),
		*ActionData.ActionName, ActionData.CasterId, ActionData.TargetId);

	// Find the source and target based on their IDs
	ABasicPlayer* SourcePlayer = GetPlayerByCharacterId(ActionData.CasterId);
	
	if (SourcePlayer)
	{
		// Handle action on player character (e.g., play attack animation)
		UE_LOG(LogTemp, Warning, TEXT("Player %d performing action %s"), 
			ActionData.CasterId, *ActionData.ActionName);
		// TODO: Implement action performing in BasicPlayer class
		// SourcePlayer->PerformAction(ActionData.ActionName);
	}
	
	// Note: The target's response to the action (e.g., taking damage) will be handled 
	// through the combatResult event and the respective update health methods
}

void UMyGameInstance::UpdateMobHealth(int32 TargetId, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Updating mob health for ID %d: Health=%d, IsDead=%s, Damage=%d"),
		TargetId, NewHealth, bIsDead ? TEXT("True") : TEXT("False"), DamageDealt);

	// Use new combat system to find and update MOB
	if (UCombatSystemManager* CombatManager = GetCombatSystemManager())
	{
		TScriptInterface<ICombatable> MobCombatable = CombatManager->FindCombatableById(TargetId, ECasterType::Mob);
		if (MobCombatable.GetInterface() && MobCombatable.GetObject() && IsValid(MobCombatable.GetObject()))
		{
			ICombatable* MobInterface = MobCombatable.GetInterface();
			
			// Update through combat system
			MobInterface->SetCurrentHealth_Implementation(NewHealth);
			MobInterface->SetCurrentMana_Implementation(NewMana);
			
			if (bIsDead)
			{
				MobInterface->SetDead_Implementation(true);
			}
			
			// Show damage effect if damaged
			//if (bIsDamaged && DamageDealt > 0)
			//{
			//	MobInterface->ShowDamageEffect_Implementation(DamageDealt, false, ESkillSchool::Physical);
			//}
			
			UE_LOG(LogTemp, Warning, TEXT("Updated MOB through combat system"));
			return;
		}
	}

}

void UMyGameInstance::UpdatePlayerHealth(int32 TargetId, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Updating player health for ID %d: Health=%d, IsDead=%s, Damage=%d"),
		TargetId, NewHealth, bIsDead ? TEXT("True") : TEXT("False"), DamageDealt);

	// Use new combat system to find and update Player
	if (UCombatSystemManager* CombatManager = GetCombatSystemManager())
	{
		TScriptInterface<ICombatable> PlayerCombatable = CombatManager->FindCombatableById(TargetId, ECasterType::Player);
		if (PlayerCombatable.GetInterface() && PlayerCombatable.GetObject() && IsValid(PlayerCombatable.GetObject()))
		{
			ICombatable* PlayerInterface = PlayerCombatable.GetInterface();
			
			// Update through combat system
			PlayerInterface->SetCurrentHealth_Implementation(NewHealth);
			PlayerInterface->SetCurrentMana_Implementation(NewMana);
			
			// Show damage effect if damaged
			//if (bIsDamaged && DamageDealt > 0)
			//{
			//	PlayerInterface->ShowDamageEffect_Implementation(DamageDealt, false, ESkillSchool::Physical);
			//}
			
			// Handle player death if needed
			if (bIsDead)
			{
				PlayerInterface->SetDead_Implementation(true);
			}
			
			UE_LOG(LogTemp, Warning, TEXT("Updated Player through combat system"));
			return;
		}
	}
}

void UMyGameInstance::UpdateTargetHealth(int32 TargetId, int32 TargetType, const FString& TargetTypeString, 
	int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Updating target health for %s ID %d: Health=%d, IsDead=%s, Damage=%d"),
		*TargetTypeString, TargetId, NewHealth, bIsDead ? TEXT("True") : TEXT("False"), DamageDealt);

	// Route the update based on target type
	if (TargetTypeString.Equals("mob", ESearchCase::IgnoreCase) || 
        TargetTypeString.Equals("MOB", ESearchCase::IgnoreCase) || 
        TargetType == 1 || 
        TargetType == 3)  // Support both numerical and string type identifiers
	{
		UpdateMobHealth(TargetId, NewHealth, NewMana, bIsDead, bIsDamaged, DamageDealt);
	}
	else if (TargetTypeString.Equals("player", ESearchCase::IgnoreCase) || 
             TargetTypeString.Equals("PLAYER", ESearchCase::IgnoreCase) || 
             TargetType == 2)
	{
		UpdatePlayerHealth(TargetId, NewHealth, NewMana, bIsDead, bIsDamaged, DamageDealt);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown target type: %s (Type %d) for target %d"), 
			*TargetTypeString, TargetType, TargetId);
	}
}

void UMyGameInstance::InitGameSystems()
{
	// Initialize the AudioManager
	AudioManager = NewObject<UAudioManager>(this);
	if (AudioManager)
	{
		UE_LOG(LogTemp, Log, TEXT("AudioManager created"));
	}

	// Initialize the item manager with the visuals data table if available
	if (ItemManager && ItemVisualsDataTable)
	{
		ItemManager->LoadItemVisualsDataTable(ItemVisualsDataTable);
		UE_LOG(LogTemp, Log, TEXT("Initialized ItemManager with configured ItemVisualsDataTable"));
	}
	else if (ItemManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemVisualsDataTable not configured in editor - item visuals will use defaults"));
	}
}

void UMyGameInstance::SetInventoryManager(UInventoryManager* NewInventoryManager)
{
	if (NewInventoryManager)
	{
		InventoryManager = NewInventoryManager;
		UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Inventory Manager reference set"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Cannot set null Inventory Manager"));
	}
}

UTimeSyncService* UMyGameInstance::GetTimeSyncService()
{
	return TimeSyncService;
}

void UMyGameInstance::ProcessTimeSyncData(const FMessageDataStruct& MessageData)
{
	// Process time sync data from server response
	if (TimeSyncService && !MessageData.timestamp.IsEmpty() && MessageData.serverRecvMs > 0 && MessageData.serverSendMs > 0)
	{
		// Use timestamp as request ID since we don't have a proper request ID in the current structure
		FString RequestId = MessageData.timestamp;
		TimeSyncService->UpdateTimeSyncData(RequestId, MessageData.serverRecvMs, MessageData.serverSendMs);
		
		UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Updated time sync data - RequestId: %s, ServerRecv: %lld, ServerSend: %lld"),
			*RequestId, MessageData.serverRecvMs, MessageData.serverSendMs);
	}
}

UPlayerStatsManager* UMyGameInstance::GetPlayerStatsManager() const
{
	return PlayerStatsManager;
}

UDialogueManager* UMyGameInstance::GetDialogueManager() const
{
	return DialogueManager;
}

UQuestManager* UMyGameInstance::GetQuestManager() const
{
	return QuestManager;
}

UEquipmentManager* UMyGameInstance::GetEquipmentManager() const
{
	return EquipmentManager;
}

UVendorManager* UMyGameInstance::GetVendorManager() const
{
	return VendorManager;
}

URepairManager* UMyGameInstance::GetRepairManager() const
{
	return RepairManager;
}

UTradeManager* UMyGameInstance::GetTradeManager() const
{
	return TradeManager;
}

UBestiaryNetworkHandler* UMyGameInstance::GetBestiaryNetworkHandler() const
{
	return BestiaryNetworkHandler;
}

UChatManager* UMyGameInstance::GetChatManager() const
{
	return ChatManager;
}