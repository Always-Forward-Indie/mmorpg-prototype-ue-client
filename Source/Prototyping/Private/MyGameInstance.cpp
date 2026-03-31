// Fill out your copyright notice in the Description page of Project Settings.




#include "MyGameInstance.h"
#include "Containers/Ticker.h"
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
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Dialogue/DialogueNetworkHandler.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Quest/QuestNetworkHandler.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Trade/TradeNetworkHandler.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentNetworkHandler.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Vendor/VendorNetworkHandler.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Repair/RepairNetworkHandler.h"

#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Chat/ChatNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"

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

	// Initialize Dialogue System
	DialogueManager = NewObject<UDialogueManager>(this);
	DialogueNetworkHandler = NewObject<UDialogueNetworkHandler>(this);

	// Initialize Quest System
	QuestManager = NewObject<UQuestManager>(this);
	QuestNetworkHandler = NewObject<UQuestNetworkHandler>(this);

	// Initialize Trade System
	TradeManager = NewObject<UTradeManager>(this);
	TradeNetworkHandler = NewObject<UTradeNetworkHandler>(this);

	// Initialize Equipment System
	EquipmentManager = NewObject<UEquipmentManager>(this);
	EquipmentNetworkHandler = NewObject<UEquipmentNetworkHandler>(this);

	// Initialize Vendor System
	VendorManager = NewObject<UVendorManager>(this);
	VendorNetworkHandler = NewObject<UVendorNetworkHandler>(this);

	// Initialize Repair System
	RepairManager = NewObject<URepairManager>(this);
	RepairNetworkHandler = NewObject<URepairNetworkHandler>(this);

	// Initialize player stats manager
	PlayerStatsManager = NewObject<UPlayerStatsManager>(this);
	PlayerStatsNetworkHandler = NewObject<UPlayerStatsNetworkHandler>(this);

	// Initialize Bestiary system
	BestiaryNetworkHandler = NewObject<UBestiaryNetworkHandler>(this);

	// Initialize Chat system
	ChatManager = NewObject<UChatManager>(this);
	ChatNetworkHandler = NewObject<UChatNetworkHandler>(this);

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

	// Initialize Dialogue System
	if (DialogueManager)
	{
		DialogueManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogTemp, Warning, TEXT("DialogueManager initialized"));
	}

	if (DialogueNetworkHandler && DialogueManager)
	{
		DialogueNetworkHandler->Initialize(DialogueManager, GetNetworkManager());
		DialogueNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("DialogueNetworkHandler initialized and subscribed"));
	}

	// Initialize Quest System
	if (QuestManager)
	{
		QuestManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogTemp, Warning, TEXT("QuestManager initialized"));
	}

	if (QuestNetworkHandler && QuestManager)
	{
		QuestNetworkHandler->Initialize(QuestManager, GetNetworkManager());
		QuestNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("QuestNetworkHandler initialized and subscribed"));
	}

	// Initialize Equipment System
	if (EquipmentManager)
	{
		EquipmentManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogTemp, Warning, TEXT("EquipmentManager initialized"));
	}

	if (EquipmentNetworkHandler && EquipmentManager)
	{
		EquipmentNetworkHandler->Initialize(EquipmentManager, GetNetworkManager());
		EquipmentNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("EquipmentNetworkHandler initialized and subscribed"));
	}

	// Initialize Vendor System
	if (VendorManager)
	{
		VendorManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogTemp, Warning, TEXT("VendorManager initialized"));
	}

	if (VendorNetworkHandler && VendorManager)
	{
		VendorNetworkHandler->Initialize(VendorManager, GetNetworkManager());
		VendorNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("VendorNetworkHandler initialized and subscribed"));
	}

	// Initialize Repair System
	if (RepairManager)
	{
		RepairManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogTemp, Warning, TEXT("RepairManager initialized"));
	}

	if (RepairNetworkHandler && RepairManager)
	{
		RepairNetworkHandler->Initialize(RepairManager, GetNetworkManager());
		RepairNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("RepairNetworkHandler initialized and subscribed"));
	}

	// Initialize Trade System
	if (TradeManager)
	{
		TradeManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogTemp, Warning, TEXT("TradeManager initialized"));
	}

	if (TradeNetworkHandler && TradeManager)
	{
		TradeNetworkHandler->Initialize(TradeManager, GetNetworkManager());
		TradeNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("TradeNetworkHandler initialized and subscribed"));
	}

	// Initialize Bestiary System
	if (BestiaryNetworkHandler)
	{
		BestiaryNetworkHandler->Initialize(this, GetNetworkManager());
		BestiaryNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("BestiaryNetworkHandler initialized and subscribed"));
	}

	// Initialize PlayerStats network handler
	if (PlayerStatsNetworkHandler && PlayerStatsManager)
	{
		PlayerStatsNetworkHandler->Initialize(PlayerStatsManager, GetNetworkManager(), this);
		PlayerStatsNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("PlayerStatsNetworkHandler initialized and subscribed"));
	}

	// Initialize Chat System
	if (ChatManager)
	{
		ChatManager->Initialize(this, GetNetworkManager());
		UE_LOG(LogTemp, Warning, TEXT("ChatManager initialized"));
	}

	if (ChatNetworkHandler && ChatManager)
	{
		ChatNetworkHandler->Initialize(ChatManager, GetNetworkManager());
		ChatNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("ChatNetworkHandler initialized and subscribed"));
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
		LoadStreamingLevel(DebugLevelName);
	}
	else {
		LoadStreamingLevel(LoginLevelName);
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
	if (NetworkManager)
	{
		NetworkManager->Shutdown();
	}
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

void UMyGameInstance::LoadStreamingLevel(const FName& LevelName)
{
	// Store the level name
	LevelBeingLoaded = LevelName;

	UE_LOG(LogTemp, Warning, TEXT("LoadStreamingLevel: %s (GameInstance: %p)"), *LevelName.ToString(), this);

	// Show loading screen
	AddLoadingScreen();

	// Asynchronously load the streaming sub-level
	FLatentActionInfo LatentInfo;
	LatentInfo.CallbackTarget = this;
	LatentInfo.ExecutionFunction = "OnLoginLevelLoaded";
	LatentInfo.Linkage = 0;
	LatentInfo.UUID = 1;

	UWorld* TargetWorld = GetWorld();
	if (TargetWorld)
	{
		UGameplayStatics::LoadStreamLevel(TargetWorld, LevelName, true, true, LatentInfo);
	}
}

void UMyGameInstance::TransitionToGameWorld()
{
	if (GameWorldMap.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("TransitionToGameWorld: GameWorldMap is not set! Assign it in the GameInstance Blueprint defaults."));
		return;
	}

	if (bTransitioningToGameWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("TransitionToGameWorld: Already transitioning, ignoring duplicate call"));
		return;
	}

	// Extract just the map name from the soft object path
	// E.g., "/Game/Maps/WorldMapV1.WorldMapV1" -> "WorldMapV1"
	FString MapAssetPath = GameWorldMap.ToSoftObjectPath().GetAssetPath().ToString();
	FString MapName = FPackageName::GetShortName(MapAssetPath);
	
	// If the asset path ends with .MapName (e.g., WorldMapV1.WorldMapV1), strip the extension
	if (MapName.Contains(TEXT(".")))
	{
		MapName = MapName.Left(MapName.Find(TEXT(".")));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("TransitionToGameWorld: Starting transition to map '%s' (from path '%s')"), 
		*MapName, *MapAssetPath);

	bTransitioningToGameWorld = true;
	bGameWorldReady = false;

	// Clean up Login level UI and actors before the level switch
	RemoveLoginWidgetFromViewport();
	if (LoginLevelCamera)
	{
		LoginLevelCamera->StopSound();
		LoginLevelCamera->Destroy();
		LoginLevelCamera = nullptr;
	}

	// Show the loading screen before the transition so it is visible during the map load.
	// We tear it down first (if stale) then re-add so AddLoadingScreen's guard works correctly.
	if (LoadingScreenActor)
	{
		LoadingScreenActor->StopSound();
		LoadingScreenActor->Destroy();
		LoadingScreenActor = nullptr;
	}
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
	}
	AddLoadingScreen();

	// Also clear the monitor stats widget as it will be recreated later
	if (MonitorStatsWidget)
	{
		MonitorStatsWidget->RemoveFromParent();
		MonitorStatsWidget = nullptr;
	}

	// Clear spawned actor references — the old world (and its actors) will be destroyed.
	// ConnectedPlayers / PendingSpawnClientId / PendingRemotePlayerSpawns intentionally
	// survive the transition: they carry the data needed to spawn players once the new
	// game world is ready (ProcessPendingSpawns reads them in OnGameWorldReady).
	SpawnedPlayers.Empty();
	Player = nullptr;

	// Travel to the game world map.
	// In PIE, UGameplayStatics::OpenLevel triggers a PKG_PlayInEditor assertion on World
	// Partition external actor packages. ServerTravel is the PIE-safe alternative and
	// works identically in packaged builds.
	UWorld* CurrentWorld = GetWorld();
	if (CurrentWorld)
	{
		CurrentWorld->ServerTravel(MapName, true);

		// After ServerTravel the current world will be replaced and a new one created.
		// Poll for the new world to become ready. We use a core ticker so the delegate
		// survives the world's own timer manager being torn down.
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
			{
				CheckGameWorldReady();
				// Return false to remove the ticker once the world is ready
				return !bGameWorldReady;
			}),
			0.2f // Poll every 200ms
		);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("TransitionToGameWorld: GetWorld() returned nullptr"));
		bTransitioningToGameWorld = false;
	}
}

// This function is called when a streaming sub-level finishes loading (Login or Debug)
void UMyGameInstance::OnLoginLevelLoaded()
{
	UE_LOG(LogTemp, Warning, TEXT("OnLoginLevelLoaded: %s"), *LevelBeingLoaded.ToString());

	// Handle Login level
	if (LevelBeingLoaded == LoginLevelName)
	{
		AddLoginWidgetToViewport();
		AddMonitorStatsWidgetToViewport();

		if (GetWorld() && GetWorld()->GetFirstPlayerController())
		{
			GetWorld()->GetFirstPlayerController()->bShowMouseCursor = true;
		}

		LoginLevelCamera = GetWorld()->SpawnActor<AMyCameraActor>(LoginCameraClass, LoginLevelCameraLocation, LoginLevelCameraRotation);
		if (LoginLevelCamera)
		{
			if (GetWorld()->GetFirstPlayerController())
			{
				GetWorld()->GetFirstPlayerController()->SetViewTargetWithBlend(LoginLevelCamera, 0.5f);
				LoginLevelCamera->PlaySound(LoginMusicSoundSource);
			}
		}

		InitNetworkingSetup();
	}

	// Handle Debug level
	if (LevelBeingLoaded == DebugLevelName)
	{
		RemoveLoginWidgetFromViewport();
		if (GetWorld() && GetWorld()->GetFirstPlayerController())
		{
			GetWorld()->GetFirstPlayerController()->bShowMouseCursor = false;
		}

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

		UWorld* TargetWorld = GetWorld();
		if (TargetWorld)
		{
			FLatentActionInfo LatentInfo;
			LatentInfo.CallbackTarget = this;
			LatentInfo.Linkage = 0;
			LatentInfo.ExecutionFunction = "OnLevelUnloaded";
			LatentInfo.UUID = 2;

			UGameplayStatics::UnloadStreamLevel(TargetWorld, LoginLevelName, LatentInfo, false);
		}
	}

	// Remove loading screen
	if (LoadingScreenWidget)
	{
		const float Delay = 0.7f;
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(RemoveLoadingScreenTimerHandle, this, &UMyGameInstance::RemoveLoadingScreen, Delay, false);
		}
	}
}

void UMyGameInstance::CheckGameWorldReady()
{
	UWorld* NewWorld = GetWorld();
	if (!NewWorld) { return; }

	// Re-create the loading screen in the new world's viewport as soon as possible
	if (!LoadingScreenWidget && LoadingScreenWidgetClass)
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->AddToViewport(999);
			UE_LOG(LogTemp, Warning, TEXT("CheckGameWorldReady: Loading screen re-created in new world"));
		}
	}

	APlayerController* PC = NewWorld->GetFirstPlayerController();
	if (!PC) { return; }

	UE_LOG(LogTemp, Warning, TEXT("CheckGameWorldReady: Game world is ready!"));
	OnGameWorldReady();
}

void UMyGameInstance::OnGameWorldReady()
{
	if (bGameWorldReady) { return; }

	bGameWorldReady = true;
	bTransitioningToGameWorld = false;

	UE_LOG(LogTemp, Warning, TEXT("OnGameWorldReady: Initializing gameplay in game world"));

	UWorld* GameWorld = GetWorld();
	if (!GameWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("OnGameWorldReady: GetWorld() is null!"));
		return;
	}

	APlayerController* PC = GameWorld->GetFirstPlayerController();
	if (PC) { PC->bShowMouseCursor = false; }

	RefreshManagerWorldContexts();
	ProcessPendingSpawns();

	// Phase 3: Send playerReady ACK to server.
	// Server will auto-send Phase 4 world-state: spawnNPCs, spawnMobsInZone,
	// nearbyItems, PLAYER_EQUIPMENT_UPDATE for all online players.
	if (PlayerManager)
	{
		PlayerManager->SendPlayerReadyRequest(ClientData);
		UE_LOG(LogTemp, Warning, TEXT("OnGameWorldReady: playerReady sent to server (Phase 3)"));
	}

	if (LoadingScreenWidget)
	{
		const float Delay = 0.7f;
		GameWorld->GetTimerManager().SetTimer(RemoveLoadingScreenTimerHandle, this, &UMyGameInstance::RemoveLoadingScreen, Delay, false);
	}

	UE_LOG(LogTemp, Warning, TEXT("OnGameWorldReady: Game world initialization complete"));
}

void UMyGameInstance::RefreshManagerWorldContexts()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("RefreshManagerWorldContexts: World is null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("RefreshManagerWorldContexts: Refreshing world context on all managers"));

	if (NetworkManager)
	{
		NetworkManager->SetWorldContext(World);
		// Poll timers were registered in the old world's TimerManager which is now
		// destroyed. Re-register them in the new world so incoming server data is
		// still processed after the level transition.
		NetworkManager->RestartPolling();
	}
	if (PingManager)
	{
		PingManager->SetWorldContext(World);
		// Re-register ping timers in the new world's TimerManager
		PingManager->RestartPingUpdates();
	}
	if (AuthenticationManager) { AuthenticationManager->SetWorldContext(World); }
	if (PlayerManager) { PlayerManager->SetWorldContext(World); }
	if (MOBManager) { MOBManager->SetWorldContext(World); }
	if (SpawnZoneManager) { SpawnZoneManager->SetWorldContext(World); }
	if (ItemManager) { ItemManager->SetWorldContext(World); }
	if (InventoryManager) { InventoryManager->SetWorldContext(World); }
	if (HarvestManager) { HarvestManager->SetWorldContext(World); }
	if (CombatSystemManager) { CombatSystemManager->SetWorldContext(World); }
	if (NPCManager) { NPCManager->SetWorldContext(World); }
	if (TimeSyncService) { TimeSyncService->SetWorldContext(World); }
}

void UMyGameInstance::ProcessPendingSpawns()
{
	UE_LOG(LogTemp, Warning, TEXT("ProcessPendingSpawns: PendingSpawnClientId=%d, PendingRemote=%d"),
		PendingSpawnClientId, PendingRemotePlayerSpawns.Num());

	if (PendingSpawnClientId > 0)
	{
		int32 ClientIdToSpawn = PendingSpawnClientId;
		PendingSpawnClientId = 0;
		SpawnPlayerForClient(ClientIdToSpawn);
		// NOTE: No manual getConnectedCharacters/getSpawnZones needed.
		// Per protocol, server auto-sends Phase 4 world-state after playerReady ACK.
	}

	for (const FClientDataStruct& RemoteData : PendingRemotePlayerSpawns)
	{
		AddPlayerData(RemoteData.clientId, RemoteData);
		SpawnPlayerForClient(RemoteData.clientId);
	}
	PendingRemotePlayerSpawns.Empty();
}

void UMyGameInstance::OnLevelUnloaded()
{
	UE_LOG(LogTemp, Warning, TEXT("Level unloaded triggered: %s"), *LevelBeingLoaded.ToString());
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
	if (!ConnectedPlayers.Contains(ClientID))
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnPlayerForClient: ClientID %d not in ConnectedPlayers"), ClientID);
		return;
	}

	if (SpawnedPlayers.Contains(ClientID))
	{
		ABasicPlayer* Existing = SpawnedPlayers[ClientID];
		if (IsValid(Existing) && !Existing->IsActorBeingDestroyed())
		{
			UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForClient: ClientID %d already spawned, skipping"), ClientID);
			return;
		}
		SpawnedPlayers.Remove(ClientID);
	}

	FClientDataStruct PlayerData = ConnectedPlayers[ClientID];
	const bool bIsLocal = (ClientID == CurrentClientID);

	FVector SpawnLocation(
		PlayerData.characterData.characterPosition.positionX,
		PlayerData.characterData.characterPosition.positionY,
		PlayerData.characterData.characterPosition.positionZ);
	FRotator SpawnRotation(0.0f, PlayerData.characterData.characterPosition.rotationZ, 0.0f);

	ABasicPlayer* NewPlayer = GetWorld()->SpawnActor<ABasicPlayer>(MainPlayerClass, SpawnLocation, SpawnRotation);
	if (!NewPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnPlayerForClient: Failed to spawn ABasicPlayer for ClientID %d"), ClientID);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForClient: Spawned ClientID=%d CharID=%d IsLocal=%d Pos=(%.0f,%.0f,%.0f)"),
		ClientID, PlayerData.characterData.characterId, bIsLocal,
		SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);

	NewPlayer->SetIsOtherClient(!bIsLocal);
	NewPlayer->SetClientID(PlayerData.clientId);
	NewPlayer->SetCharacterID(PlayerData.characterData.characterId);
	NewPlayer->SetCoordinates(
		PlayerData.characterData.characterPosition.positionX,
		PlayerData.characterData.characterPosition.positionY,
		PlayerData.characterData.characterPosition.positionZ,
		PlayerData.characterData.characterPosition.rotationZ);
	NewPlayer->SetPlayerName(PlayerData.characterData.characterName);
	NewPlayer->SetPlayerClass(PlayerData.characterData.characterClass);
	NewPlayer->SetPlayerRace(PlayerData.characterData.characterRace);
	NewPlayer->SetPlayerLevel(PlayerData.characterData.characterLevel);
	NewPlayer->SetPlayerNextLevelExp(PlayerData.characterData.characterExpForLevelEnd);
	NewPlayer->SetPlayerExpPoints(PlayerData.characterData.characterExperiencePoints);
	NewPlayer->SetPlayerCurrentHPPoints(PlayerData.characterData.characterCurrentHealth);
	NewPlayer->SetPlayerCurrentMPPoints(PlayerData.characterData.characterCurrentMana);
	NewPlayer->SetPlayerAttributes(PlayerData.characterData.characterAttributes.attributesData);
	NewPlayer->SetPlayerTag(*FString::FromInt(PlayerData.characterData.characterId));
	NewPlayer->SetPlayerTag(TEXT("Player"));

	// Apply dead state from server before UI is shown so the death screen
	// appears immediately if the character was dead at login.
	if (PlayerData.characterData.bIsDead)
	{
		NewPlayer->SetDead_Implementation(true);
		UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForClient: CharID=%d spawned as DEAD"), PlayerData.characterData.characterId);
	}

	if (CombatSystemManager && PlayerData.characterData.characterId > 0 &&
		IsValid(NewPlayer) && !NewPlayer->IsActorBeingDestroyed())
	{
		TScriptInterface<ICombatable> CombatableInterface;
		CombatableInterface.SetObject(NewPlayer);
		CombatableInterface.SetInterface(Cast<ICombatable>(NewPlayer));
		CombatSystemManager->RegisterCombatable(CombatableInterface);
		UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForClient: Registered CharID=%d with CombatSystem"), PlayerData.characterData.characterId);
	}

	if (MOBManager && PlayerData.characterData.characterId > 0)
	{
		MOBManager->RegisterPlayer(PlayerData.characterData.characterId, NewPlayer);
	}

	SpawnedPlayers.Add(ClientID, NewPlayer);

	if (bIsLocal)
	{
		Player = NewPlayer;
		Player->SetIsOtherClient(false);
		NewPlayer->SetClientLogin(PlayerData.clientLogin);
		NewPlayer->SetClientSecret(PlayerData.hash);

		// GetFirstLocalPlayerController() is on UGameInstance and is PIE-safe:
		// returns the controller owned by THIS GameInstance's LocalPlayer,
		// not always Player 1's controller.
		APlayerController* PC = GetFirstLocalPlayerController(GetWorld());
		if (PC)
		{
			PC->Possess(Player);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnPlayerForClient: No local PlayerController for ClientID=%d"), ClientID);
		}

		if (InventoryManager)
		{
			// Stamp the owner character ID on the global InventoryManager so it only
			// processes packets for our local character, not other players on the same feed.
			InventoryManager->SetOwnerCharacterId(PlayerData.characterData.characterId);
			InventoryManager->RequestInventoryData(PlayerData.characterData.characterId);
			UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForClient: Requested inventory for CharID=%d"), PlayerData.characterData.characterId);
		}
	}
	else
	{
		// Remote player: bind equipment visuals to PLAYER_EQUIPMENT_UPDATE packets,
		// filtered per character ID so each remote player only sees its own updates.
		UEquipmentVisualComponent* VisComp = NewPlayer->GetEquipmentVisualComponent();
		if (VisComp && EquipmentManager && ItemManager)
		{
			const int32 RemoteCharId = PlayerData.characterData.characterId;
			VisComp->SetOwnerCharacterId(RemoteCharId);
			VisComp->InitializeForRemotePlayer(ItemManager);
			EquipmentManager->OnRemoteEquipmentStateReceivedDelegate.AddDynamic(
				VisComp, &UEquipmentVisualComponent::HandleRemoteEquipmentState);
			UE_LOG(LogTemp, Log, TEXT("SpawnPlayerForClient: Equipment visuals bound for remote CharID=%d"), RemoteCharId);
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
    
    // PRE-PHASE: Send joinGameClient to Game Server first.
    // The Game Server will respond with chunkServerData, and the response handler
    // (ProcessGameServerData) will chain to Phase 1 (joinGameClient on Chunk Server),
    // then Phase 1 continued (joinGameCharacter on Chunk Server).
    if (PlayerManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("PRE-PHASE: Sending joinGameClient to Game Server for character %d"), CurrentCharacterID);
        PlayerManager->SendJoinGameRequest(ClientData);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot join game: PlayerManager is null."));
    }
}

void UMyGameInstance::AddPlayerData(int32 ClientID, const FClientDataStruct clientData)
{
	if (ConnectedPlayers.Contains(ClientID))
	{
		// Update existing entry so a reconnecting player gets fresh data.
		// If the player actor is already spawned we leave it in place;
		// SpawnPlayerForClient guards against double-spawning.
		ConnectedPlayers[ClientID] = clientData;
		UE_LOG(LogTemp, Warning, TEXT("AddPlayerData: Updated existing entry for ClientID=%d CharID=%d"),
			clientData.clientId, clientData.characterData.characterId);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AddPlayerData: Added ClientID=%d CharID=%d"),
			clientData.clientId, clientData.characterData.characterId);
		ConnectedPlayers.Add(ClientID, clientData);
	}
}

void UMyGameInstance::RemovePlayerData(int32 ClientID)
{
	// Check if the player ID exists
	if (ConnectedPlayers.Contains(ClientID))
	{
		ConnectedPlayers.Remove(ClientID);

		// If the player was spawned, clean up delegate bindings before destroying
		if (SpawnedPlayers.Contains(ClientID))
		{
			ABasicPlayer* PlayerToRemove = SpawnedPlayers[ClientID];
			if (IsValid(PlayerToRemove) && !PlayerToRemove->IsActorBeingDestroyed())
			{
				// Unsubscribe remote equipment visual component from the delegate
				// to prevent dangling callbacks after actor destruction.
				UEquipmentVisualComponent* VisComp = PlayerToRemove->GetEquipmentVisualComponent();
				if (VisComp && EquipmentManager)
				{
					EquipmentManager->OnRemoteEquipmentStateReceivedDelegate.RemoveDynamic(
						VisComp, &UEquipmentVisualComponent::HandleRemoteEquipmentState);
				}

				// Unregister from combat system
				if (CombatSystemManager)
				{
					TScriptInterface<ICombatable> CombatableInterface;
					CombatableInterface.SetObject(PlayerToRemove);
					CombatableInterface.SetInterface(Cast<ICombatable>(PlayerToRemove));
					CombatSystemManager->UnregisterCombatable(CombatableInterface);
				}

				// Unregister from MOBManager player registry
				if (MOBManager)
				{
					const int32 CharId = PlayerToRemove->GetPlayerCharacterID();
					if (CharId > 0)
					{
						MOBManager->UnregisterPlayer(CharId);
					}
				}

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
	if (!LoginScreenWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to set character items - LoginScreenWidget is null"));
		return;
	}

	UListView* CharacterListView = LoginScreenWidget->GetCharactersListView();

	if (CharacterListView)
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
