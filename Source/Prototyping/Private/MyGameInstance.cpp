// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Components/ArrowComponent.h"
#include "DevMode/DevModeDataProvider.h"
#include "DevMode/DevModeConsoleCommands.h"
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
#include "Gameplay/Player/MasteryManager.h"
#include "Gameplay/Player/MasteryNetworkHandler.h"
#include "Gameplay/Player/ReputationManager.h"
#include "Gameplay/Player/ReputationNetworkHandler.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Player/TitleNetworkHandler.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Gameplay/NPCs/AmbientSpeechNetworkHandler.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Gameplay/WorldObjects/WIONetworkHandler.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
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
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Vendor/VendorNetworkHandler.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Repair/RepairNetworkHandler.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/SkillShop/SkillShopNetworkHandler.h"
#include "Gameplay/Characters/CharacterPreviewManager.h"
#include "Gameplay/LoginLevel/LoginLevelSetupActor.h"

#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Chat/ChatNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/ICursor.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "Engine/GameViewportClient.h"
#include "Camera/CameraComponent.h"
#include "UObject/UObjectGlobals.h"
#include "Audio/MusicZoneActor.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

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

	// Initialize Skill Shop System (NPC trainer)
	SkillShopManager = NewObject<USkillShopManager>(this);
	SkillShopNetworkHandler = NewObject<USkillShopNetworkHandler>(this);

	// Initialize player stats manager
	PlayerStatsManager = NewObject<UPlayerStatsManager>(this);
	PlayerStatsNetworkHandler = NewObject<UPlayerStatsNetworkHandler>(this);

	// Initialize Mastery system
	MasteryManager = NewObject<UMasteryManager>(this);
	MasteryNetworkHandler = NewObject<UMasteryNetworkHandler>(this);

	// Initialize Reputation system
	ReputationManager = NewObject<UReputationManager>(this);
	ReputationNetworkHandler = NewObject<UReputationNetworkHandler>(this);

	// Initialize Titles system
	TitleManager = NewObject<UTitleManager>(this);
	TitleNetworkHandler = NewObject<UTitleNetworkHandler>(this);

	// Initialize Emote system
	EmoteManager = NewObject<UEmoteManager>(this);
	EmoteNetworkHandler = NewObject<UEmoteNetworkHandler>(this);

	// Initialize NPC Ambient Speech system
	AmbientSpeechManager = NewObject<UAmbientSpeechManager>(this);
	AmbientSpeechNetworkHandler = NewObject<UAmbientSpeechNetworkHandler>(this);

	// Initialize World Interactive Objects system
	WorldObjectManager = NewObject<UWorldObjectManager>(this);
	WIONetworkHandler = NewObject<UWIONetworkHandler>(this);

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

	// Initialize localization subsystem with the configured data asset
	if (ULocalizationSubsystem* LocSys = GetSubsystem<ULocalizationSubsystem>())
	{
		LocSys->SetLocalizationData(LocalizationDataAsset);
	}

	// Build OS cursor handles from WorldInteractionConfig once, before the login level loads.
	// Handles survive level transitions so they're available immediately in game world too.
	PreloadWorldInteractionCursors();

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

	// Null out worldContext on all managers before the old world is torn down.
	// This prevents stale TObjectPtr<UWorld> handle crashes (0xFFFFFFFFFFFFFFFF)
	// when in-flight network packets arrive during level transition.
	PreLoadMapDelegateHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(
		this, &UMyGameInstance::InvalidateManagerWorldContexts);
	InitGameSystems();
}

void UMyGameInstance::OnStart()
{
	Super::OnStart();

	// The first level (login level) has finished loading and the game viewport
	// widget is now fully in the Slate hierarchy.  Give Slate focus to the game
	// viewport so the custom cursor shapes set in PreloadWorldInteractionCursors()
	// are visible immediately — without this the OS cursor shows until the user
	// clicks inside the PIE window.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
		UE_LOG(LogTemp, Log, TEXT("GameInstance: OnStart — viewport focus set, custom cursor active."));
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
		NetworkManager->ConnectChunkServer();
		// Start polling login server responses — requires WorldContext to be set first.
		// InitGameSystems() also calls this but runs from Init() where WorldContext is still
		// null, so the timer is never actually registered. We must call it here, after
		// SetWorldContext(), so PollLoginServerNetworkData() is ticked and responses
		// from register/login/etc. are dispatched to ProcessLoginResponse.
		NetworkManager->StartPollingLoginServer();
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

			// Provide GameInstance so the handler can build outbound packets (setSkillBarSlot)
			if (PlayerSkillNetworkHandler)
			{
				PlayerSkillNetworkHandler->SetGameInstance(this);
			}

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

	// Initialize EntityAudioRepository
	EntityAudioRepositoryRef = NewObject<UEntityAudioRepository>(this);
	if (EntityAudioRepositoryRef)
	{
	EntityAudioRepositoryRef->Initialize(EntityAudioProfilesTable);
		EntityAudioRepositoryRef->InitializeSkillVoiceOverrides(EntitySkillVoiceOverridesTable);
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

	// Initialize Skill Shop System (NPC trainer)
	// Must be after PlayerSkillManager is created by PlayerSkillSystemFactory
	if (SkillShopManager)
	{
		SkillShopManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogTemp, Warning, TEXT("SkillShopManager initialized"));
	}

	if (SkillShopNetworkHandler && SkillShopManager)
	{
		SkillShopNetworkHandler->Initialize(SkillShopManager, GetNetworkManager(), PlayerSkillManager);
		SkillShopNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("SkillShopNetworkHandler initialized and subscribed"));
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

	// Initialize Mastery system
	if (MasteryManager && MasteryNetworkHandler)
	{
		MasteryNetworkHandler->Initialize(MasteryManager, GetNetworkManager(), this);
		MasteryNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("MasteryNetworkHandler initialized and subscribed"));
	}

	// Initialize Reputation system
	if (ReputationManager && ReputationNetworkHandler)
	{
		ReputationNetworkHandler->Initialize(ReputationManager, GetNetworkManager(), this);
		ReputationNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("ReputationNetworkHandler initialized and subscribed"));
	}

	// Initialize Titles system
	if (TitleManager && TitleNetworkHandler)
	{
		TitleNetworkHandler->Initialize(TitleManager, GetNetworkManager(), this);
		TitleNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("TitleNetworkHandler initialized and subscribed"));
	}

	// Initialize Emote system
	if (EmoteManager && EmoteNetworkHandler)
	{
		EmoteNetworkHandler->Initialize(EmoteManager, GetNetworkManager(), this);
		EmoteNetworkHandler->SubscribeToNetworkEvents();

		// Route emoteAction broadcasts to the correct ABasicPlayer in the world
		EmoteManager->OnEmoteActionReceived.AddDynamic(this, &UMyGameInstance::RouteEmoteActionToPlayer);

		UE_LOG(LogTemp, Warning, TEXT("EmoteNetworkHandler initialized and subscribed"));
	}

	// Initialize NPC Ambient Speech system
	if (AmbientSpeechManager && AmbientSpeechNetworkHandler)
	{
		AmbientSpeechManager->Initialize(this);
		AmbientSpeechNetworkHandler->Initialize(AmbientSpeechManager, GetNetworkManager());
		AmbientSpeechNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("AmbientSpeechNetworkHandler initialized and subscribed"));
	}

	// Initialize World Interactive Objects system
	if (WorldObjectManager)
	{
		WorldObjectManager->SetWorldContext(GetWorld());
		WorldObjectManager->Initialize(GetNetworkManager(), this);

		if (WIODefinitionTable)
		{
			WorldObjectManager->SetDefinitionTable(WIODefinitionTable);
			UE_LOG(LogTemp, Warning, TEXT("WorldObjectManager: WIODefinitionTable assigned from GameInstance"));
		}
		if (WIODefaultActorClass)
		{
			WorldObjectManager->DefaultActorClass = WIODefaultActorClass;
			UE_LOG(LogTemp, Warning, TEXT("WorldObjectManager: WIODefaultActorClass assigned from GameInstance"));
		}

		UE_LOG(LogTemp, Warning, TEXT("WorldObjectManager initialized"));
	}

	if (WIONetworkHandler && WorldObjectManager)
	{
		WIONetworkHandler->Initialize(WorldObjectManager, GetNetworkManager());
		WIONetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogTemp, Warning, TEXT("WIONetworkHandler initialized and subscribed"));
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
	// Reset join flow guard so the player can select a character again after returning to login
	bJoinGameInProgress = false;

	if (DevModeConfig.bEnabled)
	{
		// DevMode: skip login, go straight to the configured level.
		// LevelOverride takes priority; fall back to DebugLevelName when not set.
		const FName DevLevelName = (DevModeConfig.LevelOverride != NAME_None)
			? DevModeConfig.LevelOverride
			: DebugLevelName;
		LoadStreamingLevel(DevLevelName);
	}
	else if (bDebug)
	{
		LoadStreamingLevel(DebugLevelName);
	}
	else
	{
		LoadStreamingLevel(LoginLevelName);
	}
}

// Process-level counter: tracks how many PIE instances are currently inside
// World Partition GenerateStreaming (i.e. between OpenLevel and OnGameWorldReady).
// When > 0 a second OpenLevel call would corrupt WorldDataLayers (UE5 WP bug).
// Both PIE instances run in the same process so a file-scope static is safe here.
static volatile int32 GActiveWorldPartitionTransitions = 0;

void UMyGameInstance::PreloadWorldInteractionCursors()
{
	if (!WorldInteractionConfig)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GameInstance: WorldInteractionConfig not assigned. "
			     "Open BP_GameInstance → Details → World Interaction → assign DA_WorldInteractionConfig. "
			     "Custom cursors will not show until this is set."));
		return;
	}

	// Build the default (empty-world) cursor.
	PreloadedDefaultCursorHandle = UCursorInteractionComponent::BuildCursorHandle(
		WorldInteractionConfig->DefaultCursor);

	// Build per-interactable-type cursors.
	for (const auto& Pair : WorldInteractionConfig->InteractionCursors)
	{
		void* Handle = UCursorInteractionComponent::BuildCursorHandle(Pair.Value);
		if (Handle)
		{
			PreloadedCursorHandles.Add(static_cast<uint8>(Pair.Key), Handle);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("GameInstance: Cursor preload complete. Default=%s, TypedHandles=%d"),
		PreloadedDefaultCursorHandle ? TEXT("OK") : TEXT("FAILED — check texture settings"),
		PreloadedCursorHandles.Num());
}

void UMyGameInstance::Shutdown()
{
	if (PreLoadMapDelegateHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapDelegateHandle);
		PreLoadMapDelegateHandle.Reset();
	}

	// If this instance held the World Partition transition slot and is shutting
	// down before OnGameWorldReady fires, release it so other PIE instances
	// are not left waiting forever on their retry timer.
	if (bTransitioningToGameWorld)
	{
		FPlatformAtomics::InterlockedExchange(&GActiveWorldPartitionTransitions, 0);
		UE_LOG(LogTemp, Warning, TEXT("Shutdown: released WP transition slot (shutdown during transition)"));
	}

	// Only send disconnect packets when this PIE instance was actually authenticated.
	// Guards against: (a) PIE stopped before login, (b) crash before auth completes.
	// Each PIE instance owns its own UMyGameInstance so ClientData is per-instance.
	const bool bIsAuthenticated = (ClientData.clientId > 0 && !ClientData.hash.IsEmpty());

	if (bIsAuthenticated)
	{
		UE_LOG(LogTemp, Warning, TEXT("Shutdown: Sending disconnect for ClientID=%d CharID=%d"),
			ClientData.clientId, ClientData.characterData.characterId);

		// Notify Login Server
		if (AuthenticationManager)
		{
			AuthenticationManager->SendLeaveGameRequest(ClientData);
		}

		// Notify Chunk Server (and optionally Game Server)
		if (PlayerManager)
		{
			PlayerManager->SendLeaveGameRequest(ClientData);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Shutdown: Skipping disconnect — ClientID=%d (not authenticated)"),
			ClientData.clientId);
	}

	// Shutdown the network manager last so the sender threads can drain
	// the disconnect packets enqueued above before sockets are closed.
	if (NetworkManager)
	{
		NetworkManager->Shutdown();
	}

	// Restore the OS-default cursor shapes we replaced in PreloadWorldInteractionCursors.
	// Without this, the custom cursor persists in the editor after PIE ends.
	if (FSlateApplication::IsInitialized())
	{
		if (TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor())
		{
#if PLATFORM_WINDOWS
			// Restore the standard Windows arrow cursor.
			// LoadCursorW(null, IDC_ARROW) returns the built-in system arrow  no files needed.
			void* ArrowCursor = reinterpret_cast<void*>(::LoadCursorW(nullptr, IDC_ARROW));
			PlatformCursor->SetTypeShape(EMouseCursor::Default, ArrowCursor);
#else
			PlatformCursor->SetTypeShape(EMouseCursor::Default, nullptr);
#endif
			PlatformCursor->SetTypeShape(EMouseCursor::Custom, nullptr);
		}
	}

	Super::Shutdown();
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

UEntityAudioRepository* UMyGameInstance::GetEntityAudioRepository()
{
	return EntityAudioRepositoryRef;
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
	bPendingSpawnDispatched = false;
	ReadyFlags = 0;
	RenderedFrameCount.Store(0);
	bLoadingScreenRemovePending.Store(false);

	// Unsubscribe any leftover render-frame delegate from a previous session.
	if (EndFrameDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrameRT.Remove(EndFrameDelegateHandle);
		EndFrameDelegateHandle.Reset();
	}
	if (GTRemoveTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GTRemoveTicker);
		GTRemoveTicker.Reset();
	}

	// Reset the "first stats delivered" flag so the next session's first
	// stats_update will re-trigger the loading screen gate.
	// Also refreshes the cached local character ID for the incoming session.
	if (PlayerStatsNetworkHandler)
	{
		PlayerStatsNetworkHandler->ResetFirstStatsFlag(CurrentCharacterID);
	}

	// Clean up Login level UI and actors before the level switch
	RemoveLoginWidgetFromViewport();
	if (CharacterPreviewManager)
	{
		CharacterPreviewManager->Cleanup();
	}
	if (LoginLevelCamera)
	{
		LoginLevelCamera->StopSound();
		LoginLevelCamera->Destroy();
		LoginLevelCamera = nullptr;
	}
	// Stop all music before the level transition.  The new world may have no
	// MusicZoneActor, and we must not let a playlist survive across the transition.
	if (AudioManager)
	{
		AudioManager->StopMusic(0.5f);
	}

	// Show the loading screen before the transition.
	// AddLoadingScreen uses GameViewportClient::AddViewportWidgetContent,
	// which is world-independent and survives level transitions (OpenLevel).
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

	DoOpenLevel();
}

void UMyGameInstance::DoOpenLevel()
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("DoOpenLevel: GetWorld() returned nullptr"));
		bTransitioningToGameWorld = false;
		return;
	}

	// Guard against concurrent World Partition GenerateStreaming across PIE instances.
	// If another instance is already transitioning, retry after a short delay.
	const int32 ActiveNow = FPlatformAtomics::InterlockedCompareExchange(
		&GActiveWorldPartitionTransitions, 1, 0);

	if (ActiveNow != 0)
	{
		// Another PIE is inside OpenLevel/GenerateStreaming — wait and retry.
		UE_LOG(LogTemp, Warning,
			TEXT("[LOADSEQ] DoOpenLevel: another PIE transition in progress, retrying in 3s"));
		CurrentWorld->GetTimerManager().SetTimer(
			OpenLevelRetryTimerHandle,
			this,
			&UMyGameInstance::DoOpenLevel,
			3.0f,
			false);
		return;
	}

	// Build the travel URL from the full asset path (e.g. "/Game/Maps/WorldMapV1")
	// stripping the object sub-path suffix if present.
	FString TravelPath = GameWorldMap.ToSoftObjectPath().GetAssetPath().ToString();
	int32 DotIdx = INDEX_NONE;
	if (TravelPath.FindLastChar(TEXT('.'), DotIdx))
	{
		TravelPath = TravelPath.Left(DotIdx);
	}

	UGameplayStatics::OpenLevel(CurrentWorld, FName(*TravelPath), true);
	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 1. OpenLevel('%s') called — ticker polling every 200ms"), *TravelPath);

	// Poll for the new world to become ready via a core ticker that survives
	// the current world's TimerManager being torn down by OpenLevel.
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
		{
			CheckGameWorldReady();
			return !bGameWorldReady;
		}),
		0.2f
	);
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

		// Login level is fully loaded and widgets are in the viewport.
		// 1. Focus the game viewport so Slate routes input (and cursor) to it.
		// 2. Re-apply SetTypeShape — the slot set in Init() can be reset when the
		//    viewport widget is created after Init() (PIE viewport init order).
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();

			if (TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor())
			{
				if (PreloadedDefaultCursorHandle)
				{
					PlatformCursor->SetTypeShape(EMouseCursor::Default, PreloadedDefaultCursorHandle);
					UE_LOG(LogTemp, Log, TEXT("GameInstance: OnLoginLevelLoaded — Default cursor slot re-applied (handle=%p)."), PreloadedDefaultCursorHandle);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("GameInstance: OnLoginLevelLoaded — PreloadedDefaultCursorHandle is null, custom cursor NOT applied. Check DA_WorldInteractionConfig."));
				}
			}
		}

		// If a LoginLevelSetupActor is placed in the level, read all camera and
		// character-slot transforms from it — overrides the Blueprint properties.
		ALoginLevelSetupActor* SetupActor = Cast<ALoginLevelSetupActor>(
			UGameplayStatics::GetActorOfClass(GetWorld(), ALoginLevelSetupActor::StaticClass()));
		if (SetupActor)
		{
			LoginLevelCameraLocation = SetupActor->LoginCameraSpot->GetComponentLocation();
			LoginLevelCameraRotation = SetupActor->LoginCameraSpot->GetComponentRotation();

			PodiumCameraLocation = SetupActor->SelectCameraSpot->GetComponentLocation();
			PodiumCameraRotation = SetupActor->SelectCameraSpot->GetComponentRotation();

			CreatePreviewCameraLocation = SetupActor->CreateCameraSpot->GetComponentLocation();
			CreatePreviewCameraRotation = SetupActor->CreateCameraSpot->GetComponentRotation();

			CreatePreviewLocation = SetupActor->CreateSlot->GetComponentLocation();
			CreatePreviewRotation = SetupActor->CreateSlot->GetComponentRotation();

			SelectedCharacterLocation = SetupActor->SelectedCharacterSlot->GetComponentLocation();
			SelectedCharacterRotation = SetupActor->SelectedCharacterSlot->GetComponentRotation();

			PodiumSpawnLocations.Reset();
			PodiumSpawnRotations.Reset();
			for (UArrowComponent* Slot : SetupActor->PodiumSlots)
			{
				if (Slot)
				{
					PodiumSpawnLocations.Add(Slot->GetComponentLocation());
					PodiumSpawnRotations.Add(Slot->GetComponentRotation());
				}
			}
			// Keep legacy fallback for anything that still reads PodiumSpawnRotation.
			if (SetupActor->PodiumSlots.Num() > 0 && SetupActor->PodiumSlots[0])
			{
				PodiumSpawnRotation = SetupActor->PodiumSlots[0]->GetComponentRotation();
			}

			UE_LOG(LogTemp, Log, TEXT("LoginLevelSetupActor found — camera and spawn transforms applied."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("LoginLevelSetupActor not found in Login Level — using Blueprint-configured values."));
		}

		LoginLevelCamera = GetWorld()->SpawnActor<AMyCameraActor>(LoginCameraClass, LoginLevelCameraLocation, LoginLevelCameraRotation);
		if (LoginLevelCamera)
		{
			if (GetWorld()->GetFirstPlayerController())
			{
				GetWorld()->GetFirstPlayerController()->SetViewTargetWithBlend(LoginLevelCamera, 0.0f);

				// Login level is now active: push the SoundMix into the real world and
				// re-apply cached volume overrides so all volume sliders work immediately.
				if (AudioManager) { AudioManager->ReapplySoundMix(); }

				// If a dedicated login playlist is configured, play it through AudioManager
				// so volume sliders work. Otherwise fall back to the legacy MyCameraActor path.
				if (AudioManager && !AudioManager->LoginPlaylistId.IsEmpty())
				{
					AudioManager->PlayPlaylist(AudioManager->LoginPlaylistId, /*bForceRestart=*/true);
				}
				else
				{
					LoginLevelCamera->PlaySound(LoginMusicSoundSource);
				}
			}
		}

		InitNetworkingSetup();

		// Initialize CharacterPreviewManager for the login level
		if (!CharacterPreviewManager)
		{
			CharacterPreviewManager = NewObject<UCharacterPreviewManager>(this);
		}
		CharacterPreviewManager->Initialize(this);

		// Login UI is now in the viewport. Remove the loading screen after one tick
		// so the render thread has received the draw command first.
		if (UWorld* W = GetWorld())
		{
		W->GetTimerManager().SetTimer(RemoveLoadingScreenTimerHandle, this, &UMyGameInstance::RemoveLoadingScreen, 1.5f, false);
		}
	}

	// Handle Debug / DevMode level
	// This branch fires when:
	//   a) bDebug is set and LevelBeingLoaded == DebugLevelName, OR
	//   b) DevModeConfig.bEnabled is set and we loaded to LevelOverride (or DebugLevelName as fallback)
	const FName DevModeLevelName = (DevModeConfig.bEnabled && DevModeConfig.LevelOverride != NAME_None)
		? DevModeConfig.LevelOverride
		: DebugLevelName;

	if (LevelBeingLoaded == DevModeLevelName)
	{
		RemoveLoginWidgetFromViewport();
		if (GetWorld() && GetWorld()->GetFirstPlayerController())
		{
			GetWorld()->GetFirstPlayerController()->bShowMouseCursor = false;
		}

		FClientDataStruct clientData;

		if (DevModeConfig.bEnabled)
		{
			// Instantiate the data provider if not already created
			if (!DevModeDataProvider)
			{
				DevModeDataProvider = NewObject<UDevModeDataProvider>(this);
				DevModeDataProvider->Initialize(this, DevModeConfig);
			}

			if (!DevModeDataProvider->LoadPlayerData(clientData))
			{
				// JSON load failed — fall back to minimal stub so the level is still playable
				UE_LOG(LogTemp, Warning, TEXT("DevMode: LoadPlayerData failed — using stub data. Check Config/DevMode/dev_player.json"));
				clientData.clientId = 1;
				clientData.characterData.characterId = 1;
				clientData.characterData.characterPosition.positionX = 0.0f;
				clientData.characterData.characterPosition.positionY = 0.0f;
				clientData.characterData.characterPosition.positionZ = 90.0f;
			}
		}
		else
		{
			// Legacy bDebug path — hardcoded stub
			clientData.clientId = 1;
			clientData.characterData.characterId = 1;
			clientData.characterData.characterPosition.positionX = 0.0f;
			clientData.characterData.characterPosition.positionY = 0.0f;
			clientData.characterData.characterPosition.positionZ = 90.0f;
		}

		// ── "Play from here" ────────────────────────────────────────────────
		// Override characterPosition with the active editor viewport camera
		// position so the player spawns where you placed the camera in the editor.
		// Only runs in editor builds when the flag is set.
#if WITH_EDITOR
		if (DevModeConfig.bSpawnAtEditorCameraPosition && GEditor)
		{
			FVector    CamLoc  = FVector::ZeroVector;
			FRotator   CamRot  = FRotator::ZeroRotator;
			bool       bFoundCamera = false;

			// Iterate all level viewports to find the perspective one that PIE was launched from.
			for (FEditorViewportClient* VC : GEditor->GetAllViewportClients())
			{
				if (VC && VC->IsPerspective() && !VC->IsSimulateInEditorViewport())
				{
					CamLoc = VC->GetViewLocation();
					CamRot = VC->GetViewRotation();
					bFoundCamera = true;
					break;
				}
			}

			if (bFoundCamera)
			{
				clientData.characterData.characterPosition.positionX = CamLoc.X;
				clientData.characterData.characterPosition.positionY = CamLoc.Y;
				clientData.characterData.characterPosition.positionZ = CamLoc.Z;
				clientData.characterData.characterPosition.rotationZ  = CamRot.Yaw;

				UE_LOG(LogTemp, Log,
					TEXT("DevMode: SpawnAtEditorCameraPosition → (%.0f, %.0f, %.0f) Yaw=%.1f"),
					CamLoc.X, CamLoc.Y, CamLoc.Z, CamRot.Yaw);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("DevMode: SpawnAtEditorCameraPosition — no perspective viewport found, using JSON position"));
			}
		}
#endif

		CurrentCharacterID = clientData.characterData.characterId;
		CurrentClientID = clientData.clientId;

		ClientData = clientData;

		AddPlayerData(clientData.clientId, clientData);
		SpawnPlayerForClient(clientData.clientId);

		if (DevModeConfig.bEnabled && DevModeDataProvider)
		{
			// Populate test mobs
			if (DevModeConfig.bSpawnTestMobs && MOBManager)
			{
				DevModeDataProvider->PopulateMobs(MOBManager);
			}

			// Populate test inventory
			if (DevModeConfig.bPopulateInventory && InventoryManager)
			{
				DevModeDataProvider->PopulateInventory(InventoryManager, clientData.characterData.characterId);
			}

			// Register dev console commands once
			if (!DevModeConsoleCommands)
			{
				DevModeConsoleCommands = NewObject<UDevModeConsoleCommands>(this);
				DevModeConsoleCommands->RegisterCommands(this);
			}

			UE_LOG(LogTemp, Log, TEXT("DevMode: Active — player '%s' (clientId=%d, charId=%d)"),
				*clientData.characterData.characterName,
				clientData.clientId,
				clientData.characterData.characterId);
		}

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
	// NOTE: For the Debug/DevMode level the loading screen is removed here immediately
	// since there is no full login→world transition flow (no ReadyFlags system).
	if (LevelBeingLoaded == DevModeLevelName)
	{
		if (LoadingScreenWidget)
		{
			const float Delay = 0.7f;
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().SetTimer(RemoveLoadingScreenTimerHandle, this, &UMyGameInstance::RemoveLoadingScreen, Delay, false);
			}
		}
	}
	// For the normal game world transition the loading screen is managed entirely
	// by CheckGameWorldReady / CheckAllReadyFlags / StartFrameCountdown.
	// Do NOT remove it here.
}

void UMyGameInstance::CheckGameWorldReady()
{
	UWorld* NewWorld = GetWorld();
	if (!NewWorld) { return; }

	// Loading screen lives at Slate/GameViewportClient level — it survives
	// ServerTravel.  No re-creation needed; just verify it is still there.
	if (!LoadingScreenWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] CheckGameWorldReady: no loading screen widget — re-creating via AddLoadingScreen"));
		AddLoadingScreen();
	}

	// Gate 1: PlayerController must exist before we can do anything.
	APlayerController* PC = NewWorld->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Log, TEXT("[LOADSEQ] Gate1 WAIT: no PlayerController yet"));
		return;
	}

	// Spawn the pending local player exactly once as soon as the PC is ready.
	// We do this eagerly so the pawn exists and World Partition can use the
	// player's location as a streaming source to load the surrounding cells.
	if (!bPendingSpawnDispatched && (PendingSpawnClientId > 0 || PendingRemotePlayerSpawns.Num() > 0))
	{
		bPendingSpawnDispatched = true;

		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 3. Gate1 PASS: PC ready � dispatching spawn (PendingClientId=%d)"), PendingSpawnClientId);

		// World context / managers must be refreshed before spawning so the new
		// world's TimerManager / network polling are active.
		RefreshManagerWorldContexts();
		ProcessPendingSpawns();

		// Phase 3: ACK the server so it sends Phase 4 world-state.
		if (PlayerManager)
		{
			PlayerManager->SendPlayerReadyRequest(ClientData);
			UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 4. playerReady sent to server (Phase 3)"));
		}

		// Reset ready-flags and arm the safety fallback timer.
		ReadyFlags = 0;
		NewWorld->GetTimerManager().SetTimer(
			LoadingScreenSafetyTimerHandle,
			this,
			&UMyGameInstance::RemoveLoadingScreen,
			LoadingScreenSafetyTimeout,
			false);
		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] Safety timer armed (%.0fs)"), LoadingScreenSafetyTimeout);
	}

	// Gate 2: Pawn must be possessed � Possess() is called inside SpawnPlayerForClient.
	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogTemp, Log, TEXT("[LOADSEQ] Gate2 WAIT: PC exists but no Pawn yet (Possess not done)"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] Gate2 PASS: Pawn='%s' possessed at Pos=(%.0f,%.0f,%.0f)"),
		*Pawn->GetName(),
		Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z);

	// Gate 3: World Partition streaming must be complete so no geometry pops in
	// during the first rendered frame after the loading screen disappears.
	if (UWorldPartitionSubsystem* WPS = NewWorld->GetSubsystem<UWorldPartitionSubsystem>())
	{
		if (!WPS->IsAllStreamingCompleted())
		{
			UE_LOG(LogTemp, Log, TEXT("[LOADSEQ] Gate3 WAIT: World Partition still streaming..."));
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] Gate3 PASS: World Partition streaming complete"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] Gate3 SKIP: no WorldPartitionSubsystem (not a WP map)"));
	}

	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 5. All gates passed � calling OnGameWorldReady"));
	OnGameWorldReady();
}

void UMyGameInstance::OnGameWorldReady()
{
	if (bGameWorldReady) { return; }

	bGameWorldReady = true;
	bTransitioningToGameWorld = false;

	// Release the process-level World Partition transition slot so the next
	// PIE instance (if it was waiting) can now proceed with its own OpenLevel.
	FPlatformAtomics::InterlockedExchange(&GActiveWorldPartitionTransitions, 0);
	UE_LOG(LogTemp, Log, TEXT("[LOADSEQ] OnGameWorldReady: released WP transition slot"));

	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 6. OnGameWorldReady: bGameWorldReady=true, ReadyFlags=0x%02X � calling CheckAllReadyFlags"), ReadyFlags);

	UWorld* GameWorld = GetWorld();
	if (!GameWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("[LOADSEQ] OnGameWorldReady: GetWorld() is null!"));
		return;
	}

	APlayerController* PC = GameWorld->GetFirstPlayerController();
	if (PC) { PC->bShowMouseCursor = false; }

	// Phase 4 data (getConnectedCharacters, joinGameCharacter broadcasts) can arrive
	// after ProcessPendingSpawns was called at Gate 1 but before bGameWorldReady=true.
	// Any remote players queued during that window are still in PendingRemotePlayerSpawns
	// - spawn them now that the world is confirmed ready.
	if (PendingRemotePlayerSpawns.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] OnGameWorldReady: spawning %d deferred remote player(s)"), PendingRemotePlayerSpawns.Num());
		for (const FClientDataStruct& RemoteData : PendingRemotePlayerSpawns)
		{
			AddPlayerData(RemoteData.clientId, RemoteData);
			SpawnPlayerForClient(RemoteData.clientId);
		}
		PendingRemotePlayerSpawns.Empty();
	}

	// All Notify* flags may already be accumulated while streaming was running.
	// Attempt the frame countdown now in case we were the last gate to clear.
	CheckAllReadyFlags();

	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] OnGameWorldReady: complete"));
}

void UMyGameInstance::NotifyPlayerReadyAck()
{
	if (!bPendingSpawnDispatched) { return; }
	ReadyFlags |= ReadyFlag_PlayerReadyAck;
	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] FLAG PlayerReadyAck set (mask=0x%02X, bGameWorldReady=%d)"), ReadyFlags, bGameWorldReady);
	CheckAllReadyFlags();
}

void UMyGameInstance::NotifyUIInitialized()
{
	if (!bPendingSpawnDispatched) { return; }
	ReadyFlags |= ReadyFlag_UIInitialized;
	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] FLAG UIInitialized set (mask=0x%02X, bGameWorldReady=%d)"), ReadyFlags, bGameWorldReady);
	CheckAllReadyFlags();
}

void UMyGameInstance::NotifyStatsReceived()
{
	if (!bPendingSpawnDispatched) { return; }
	ReadyFlags |= ReadyFlag_StatsReceived;
	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] FLAG StatsReceived set (mask=0x%02X, bGameWorldReady=%d)"), ReadyFlags, bGameWorldReady);
	CheckAllReadyFlags();
}

void UMyGameInstance::NotifyPlayerSpawned()
{
	if (!bPendingSpawnDispatched) { return; }
	ReadyFlags |= ReadyFlag_PlayerSpawned;
	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] FLAG PlayerSpawned set (mask=0x%02X, bGameWorldReady=%d)"), ReadyFlags, bGameWorldReady);

	// Pawn is now in the world — immediately trigger the initial overlap check on
	// every MusicZoneActor so music starts without waiting for the 0.25 s poll tick.
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AMusicZoneActor> It(W); It; ++It)
		{
			It->OnPlayerSpawned();
		}
	}

	CheckAllReadyFlags();
}

void UMyGameInstance::CheckAllReadyFlags()
{
	UE_LOG(LogTemp, Log, TEXT("[LOADSEQ] CheckAllReadyFlags: mask=0x%02X required=0x%02X bGameWorldReady=%d frameCountdownActive=%d"),
		ReadyFlags, ReadyFlag_AllRequired, (int)bGameWorldReady, EndFrameDelegateHandle.IsValid() ? 1 : 0);

	if ((ReadyFlags & ReadyFlag_AllRequired) != ReadyFlag_AllRequired) { return; }
	if (!LoadingScreenWidget) { return; }
	if (EndFrameDelegateHandle.IsValid()) { return; }
	if (!bGameWorldReady)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] CheckAllReadyFlags: all flags ready but Gate3 (WP streaming) not done yet"));
		return;
	}

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(LoadingScreenSafetyTimerHandle);
		W->GetTimerManager().ClearTimer(RemoveLoadingScreenTimerHandle);
	}

	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 7. ALL flags set (0x%02X) + WP streamed � starting frame countdown (%d frames)"),
		ReadyFlags, MinRenderedFramesBeforeHide);
	StartFrameCountdown();
}
void UMyGameInstance::StartFrameCountdown()
{
	RenderedFrameCount.Store(0);
	bLoadingScreenRemovePending.Store(false);

	// Count frames on the RENDER THREAD (OnEndFrameRT) so we are certain the
	// loading screen has actually been composited by the GPU before we remove it.
	// OnEndFrameRT fires at the true end of each render frame � not just the end
	// of a game-thread tick � so it reliably reflects what the player has seen.
	EndFrameDelegateHandle = FCoreDelegates::OnEndFrameRT.AddLambda([this]()
	{
		if (bLoadingScreenRemovePending.Load()) { return; }

		const int32 NewCount = RenderedFrameCount.IncrementExchange() + 1;
		UE_LOG(LogTemp, Log, TEXT("[LOADSEQ] RT Frame %d / %d"), NewCount, MinRenderedFramesBeforeHide);

		if (NewCount >= MinRenderedFramesBeforeHide)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 8. %d RT frames seen � signalling GT to remove loading screen"), MinRenderedFramesBeforeHide);
			bLoadingScreenRemovePending.Store(true);
		}
	});

	// Poll the atomic flag on the game thread once per tick.
	// RemoveLoadingScreen touches UMG and TimerManager � GT only.
	GTRemoveTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float) -> bool
		{
			if (!bLoadingScreenRemovePending.Load()) { return true; }

			if (EndFrameDelegateHandle.IsValid())
			{
				FCoreDelegates::OnEndFrameRT.Remove(EndFrameDelegateHandle);
				EndFrameDelegateHandle.Reset();
			}

			UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 9. GT ticker: removing loading screen NOW"));
			RemoveLoadingScreen();
			return false;   // unregister ticker
		}),
		0.0f);
}

void UMyGameInstance::InvalidateManagerWorldContexts(const FString& /*MapName*/)
{
	// FCoreUObjectDelegates::PreLoadMap is a GLOBAL (static) delegate — it fires
	// for every level transition (OpenLevel / ServerTravel) in the entire process.
	// In PIE with multiple players, each client has its own GameInstance + World,
	// but they all share this delegate.  Without filtering, Player 1's transition
	// would invalidate Player 2's manager world contexts (and vice-versa),
	// breaking networking, poll timers, and actor spawning for the other client.
	//
	// Guard: only proceed if THIS GameInstance is the one currently transitioning.
	// bTransitioningToGameWorld is set to true in TransitionToGameWorld() and
	// reset in OnGameWorldReady(), so it is true exactly during our own
	// level transition.  The login-level load uses LoadStreamLevel (streaming
	// sub-level) which does NOT fire PreLoadMap, so no false positives there.
	if (!bTransitioningToGameWorld)
	{
		UE_LOG(LogTemp, Log, TEXT("InvalidateManagerWorldContexts: SKIPPED (not transitioning - PreLoadMap from another PIE instance)"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("InvalidateManagerWorldContexts: Nulling world context on all managers before map load"));

	// Set worldContext to nullptr on every manager that holds one.
	// This prevents stale TObjectPtr<UWorld> handle assertions (0xFFFFFFFFFFFFFFFF)
	// when network packets arrive while the old world is being destroyed.
	if (NetworkManager)      { NetworkManager->SetWorldContext(nullptr); }
	if (PingManager)         { PingManager->SetWorldContext(nullptr); }
	if (AuthenticationManager) { AuthenticationManager->SetWorldContext(nullptr); }
	if (PlayerManager)       { PlayerManager->SetWorldContext(nullptr); }
	if (MOBManager)          { MOBManager->SetWorldContext(nullptr); MOBManager->ClearWorldState(); }
	if (SpawnZoneManager)    { SpawnZoneManager->SetWorldContext(nullptr); SpawnZoneManager->ClearWorldState(); }
	if (ItemManager)         { ItemManager->SetWorldContext(nullptr); }
	if (InventoryManager)    { InventoryManager->SetWorldContext(nullptr); }
	if (HarvestManager)      { HarvestManager->SetWorldContext(nullptr); }
	if (CombatSystemManager) { CombatSystemManager->SetWorldContext(nullptr); }
	if (NPCManager)          { NPCManager->SetWorldContext(nullptr); }
	if (WorldObjectManager)  { WorldObjectManager->SetWorldContext(nullptr); WorldObjectManager->ClearWorldState(); }
	if (TimeSyncService)     { TimeSyncService->SetWorldContext(nullptr); }
	// Release audio components that belong to the dying world so CrossfadeToTrack
	// re-creates them in the new world instead of silently replaying on a dead device.
	if (AudioManager)        { AudioManager->InvalidateAudioComponents(); }
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

		// MonitorStatsWidget was destroyed before the level transition (TransitionToGameWorld
		// nulls it out). Re-create it here so the ping display is visible in the game world.
		if (!IsValid(MonitorStatsWidget))
		{
			AddMonitorStatsWidgetToViewport();
		}
		PingManager->Initialize(NetworkManager, MonitorStatsWidget);

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
	if (WorldObjectManager) { WorldObjectManager->SetWorldContext(World); }
	if (TimeSyncService) { TimeSyncService->SetWorldContext(World); }

	// Re-push the SoundMix and re-apply all cached volume overrides.
	// The audio device loses its SoundMix modifier stack on world teardown,
	// so without this call every volume slider becomes a no-op after a level transition.
	if (AudioManager) { AudioManager->ReapplySoundMix(); }
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
	// Logo sits at the very bottom (Z-order 5), below the login form.
	if (LoginLogoWidgetClass)
	{
		LoginLogoWidget = CreateWidget<UW_LoginLogoWidget>(this, LoginLogoWidgetClass);
		if (LoginLogoWidget)
		{
			LoginLogoWidget->AddToViewport(5);
		}
	}

	// Prefer new LoginFlowWidget if configured
	if (LoginFlowWidgetClass)
	{
		LoginFlowWidget = CreateWidget<ULoginFlowWidget>(this, LoginFlowWidgetClass);
		if (LoginFlowWidget)
		{
			// Pass error messages DataTable
			if (LoginErrorMessagesTable)
			{
				LoginFlowWidget->ErrorMessagesTable = LoginErrorMessagesTable;
			}
			// Z-order 10 keeps the login UI above the nameplate canvas (Z=1).
			LoginFlowWidget->AddToViewport(10);
		}
	}
	else if (LoginScreenWidgetClass)
	{
		// Legacy fallback
		LoginScreenWidget = CreateWidget<ULoginWidget>(this, LoginScreenWidgetClass);
		if (LoginScreenWidget)
		{
			LoginScreenWidget->AddToViewport();
		}
	}

	// Overlay sits on top of everything (Z-order 20).
	if (LoginScreenOverlayWidgetClass)
	{
		LoginScreenOverlayWidget = CreateWidget<UW_LoginScreenOverlayWidget>(this, LoginScreenOverlayWidgetClass);
		if (LoginScreenOverlayWidget)
		{
			LoginScreenOverlayWidget->AddToViewport(20);
		}
	}
}

void UMyGameInstance::RemoveLoginWidgetFromViewport()
{
	if (LoginLogoWidget)
	{
		LoginLogoWidget->RemoveFromParent();
		LoginLogoWidget = nullptr;
	}
	if (LoginFlowWidget)
	{
		LoginFlowWidget->RemoveFromParent();
		LoginFlowWidget = nullptr;
	}
	if (LoginScreenWidget)
	{
		LoginScreenWidget->RemoveFromParent();
		LoginScreenWidget = nullptr;
	}
	if (LoginScreenOverlayWidget)
	{
		LoginScreenOverlayWidget->RemoveFromParent();
		LoginScreenOverlayWidget = nullptr;
	}
	if (LoginSettingsWidget)
	{
		LoginSettingsWidget->RemoveFromParent();
		LoginSettingsWidget = nullptr;
	}
}

void UMyGameInstance::ShowLoginSettings(ESettingsTab Tab)
{
	if (!LoginSettingsWidgetClass) { return; }

	// Create once, reuse on subsequent calls.
	if (!IsValid(LoginSettingsWidget))
	{
		LoginSettingsWidget = CreateWidget<UW_SettingsWidget>(this, LoginSettingsWidgetClass);
		if (LoginSettingsWidget)
		{
			// Z-order 30: above overlay (20) and login flow (10).
			LoginSettingsWidget->AddToViewport(30);
		}
	}

	if (LoginSettingsWidget)
	{
		LoginSettingsWidget->OpenSettings(Tab);
	}
}

void UMyGameInstance::HideLoginSettings()
{
	if (IsValid(LoginSettingsWidget))
	{
		LoginSettingsWidget->CloseSettings();
	}
}

void UMyGameInstance::StartLoadingScreenMusic()
{
	if (!LoadingMusicSoundSource || !GetWorld()) { return; }

	// Already playing — nothing to do
	if (IsValid(LoadingScreenAudioComponent) && LoadingScreenAudioComponent->IsPlaying()) { return; }

	// bPersistAcrossLevelTransition=true binds the component to the raw AudioDevice
	// instead of a world actor, and sets bIgnoreForFlushing internally — so the
	// sound is NOT flushed when OpenLevel tears down the old world.
	// bAutoDestroy=false: we control the lifetime; StopLoadingScreenMusic() cleans up.
	LoadingScreenAudioComponent = UGameplayStatics::SpawnSound2D(
		GetWorld(),
		LoadingMusicSoundSource,
		/*VolumeMultiplier=*/1.0f,
		/*PitchMultiplier=*/1.0f,
		/*StartTime=*/0.0f,
		/*ConcurrencySettings=*/nullptr,
		/*bPersistAcrossLevelTransition=*/true,
		/*bAutoDestroy=*/false);

	if (LoadingScreenAudioComponent)
	{
		LoadingScreenAudioComponent->bIsUISound = true;

		// Route through MusicClass so the Music volume slider affects loading music
		if (AudioManager && AudioManager->MusicClass)
		{
			LoadingScreenAudioComponent->SoundClassOverride = AudioManager->MusicClass;
		}

		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] Loading screen music started (persists across level transition)"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] StartLoadingScreenMusic: SpawnSound2D returned null"));
	}
}

void UMyGameInstance::AddLoadingScreen()
{
	if (!LoadingScreenWidgetClass) { return; }

	// Already showing — nothing to do
	if (LoadingScreenWidget && LoadingScreenSlateWidget.IsValid()) { return; }

	UGameViewportClient* GVC = GetGameViewportClient();
	if (!GVC)
	{
		UE_LOG(LogTemp, Error, TEXT("AddLoadingScreen: no GameViewportClient"));
		return;
	}

	// Create the UMG widget if needed
	if (!LoadingScreenWidget)
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
	}
	if (!LoadingScreenWidget) { return; }

	// Take its Slate representation and add directly to the viewport overlay.
	// This is world-independent — it survives level transitions (OpenLevel).
	LoadingScreenSlateWidget = LoadingScreenWidget->TakeWidget();
	GVC->AddViewportWidgetContent(LoadingScreenSlateWidget.ToSharedRef(), MAX_int32);

	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] AddLoadingScreen: widget added via GameViewportClient (world-independent)"));

	StartLoadingScreenMusic();
}

void UMyGameInstance::RemoveLoadingScreen()
{
	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 9. RemoveLoadingScreen called � loading screen going away NOW"));

	// Clean up render-thread and GT ticker if safety timer fired before countdown finished.
	if (EndFrameDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrameRT.Remove(EndFrameDelegateHandle);
		EndFrameDelegateHandle.Reset();
	}
	if (GTRemoveTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GTRemoveTicker);
		GTRemoveTicker.Reset();
	}
	bLoadingScreenRemovePending.Store(false);

	// Stop loading music (may already be null if world was torn down by ServerTravel)
	if (IsValid(LoadingScreenAudioComponent))
	{
		LoadingScreenAudioComponent->Stop();
		LoadingScreenAudioComponent->DestroyComponent();
	}
	LoadingScreenAudioComponent = nullptr;

	// Remove from GameViewportClient (Slate level — world-independent)
	if (LoadingScreenSlateWidget.IsValid())
	{
		if (UGameViewportClient* GVC = GetGameViewportClient())
		{
			GVC->RemoveViewportWidgetContent(LoadingScreenSlateWidget.ToSharedRef());
		}
		LoadingScreenSlateWidget.Reset();
	}

	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
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

	// Use SpawnActorDeferred so we can set isOtherClient BEFORE BeginPlay fires.
	// This is critical for remote players: BeginPlay reads playerData.isOtherClient
	// to initialize interpolation targets at the correct spawn location.
	// With regular SpawnActor, isOtherClient is still false during BeginPlay,
	// causing LastReceivedPosition to be initialised to (0,0,0) instead of SpawnLocation.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABasicPlayer* NewPlayer = GetWorld()->SpawnActorDeferred<ABasicPlayer>(MainPlayerClass, FTransform(SpawnRotation, SpawnLocation), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnPlayerForClient: Failed to spawn ABasicPlayer for ClientID %d"), ClientID);
		return;
	}

	// Set isOtherClient and IDs BEFORE FinishSpawning (which calls BeginPlay).
	NewPlayer->SetIsOtherClient(!bIsLocal);
	NewPlayer->SetClientID(PlayerData.clientId);
	NewPlayer->SetCharacterID(PlayerData.characterData.characterId);

	// FinishSpawning triggers BeginPlay — at this point isOtherClient is already correct,
	// so BeginPlay initialises LastReceivedPosition = SpawnLocation (not local player pos).
	UGameplayStatics::FinishSpawningActor(NewPlayer, FTransform(SpawnRotation, SpawnLocation));

	UE_LOG(LogTemp, Warning, TEXT("SpawnPlayerForClient: Spawned ClientID=%d CharID=%d IsLocal=%d Pos=(%.0f,%.0f,%.0f)"),
		ClientID, PlayerData.characterData.characterId, bIsLocal,
		SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);

	// SetCoordinates is now a no-op for first-packet snap (bHasReceivedFirstPosition
	// was set true inside BeginPlay when isOtherClient=true and position = SpawnLocation).
	// We still call it to sync playerData fields and interpolation targets.
	NewPlayer->SetCoordinates(
		PlayerData.characterData.characterPosition.positionX,
		PlayerData.characterData.characterPosition.positionY,
		PlayerData.characterData.characterPosition.positionZ,
		PlayerData.characterData.characterPosition.rotationZ);
	NewPlayer->SetPlayerName(PlayerData.characterData.characterName);
	NewPlayer->SetPlayerClass(PlayerData.characterData.characterClass);
	NewPlayer->SetPlayerRace(PlayerData.characterData.characterRace);
	NewPlayer->SetPlayerGender(PlayerData.characterData.characterGender);
	NewPlayer->SetPlayerLevel(PlayerData.characterData.characterLevel);
	NewPlayer->SetPlayerNextLevelExp(PlayerData.characterData.characterExpForLevelEnd);
	NewPlayer->SetPlayerExpPoints(PlayerData.characterData.characterExperiencePoints);
	NewPlayer->SetPlayerCurrentHPPoints(PlayerData.characterData.characterCurrentHealth);
	NewPlayer->SetPlayerCurrentMPPoints(PlayerData.characterData.characterCurrentMana);
	NewPlayer->SetPlayerAttributes(PlayerData.characterData.characterAttributes.attributesData);
	NewPlayer->SetPlayerTag(*FString::FromInt(PlayerData.characterData.characterId));
	NewPlayer->SetPlayerTag(TEXT("Player"));

	// Apply visual from DataTable (mesh, animation, scale based on class/race/gender)
	if (CharacterVisualDefinitionsTable)
	{
		NewPlayer->ApplyVisualFromDataTable(CharacterVisualDefinitionsTable);
	}

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

	// Initialize nameplate for both local and remote players
	NewPlayer->InitialiseNameplate(bIsLocal);

	// Remote players must not block the local player's camera spring arm.
	// Disable ECC_Camera response on both the capsule and the skeletal mesh so
	// the camera never zooms in when another player walks in front of it.
	if (!bIsLocal)
	{
		if (UCapsuleComponent* Cap = NewPlayer->GetCapsuleComponent())
		{
			Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		if (USkeletalMeshComponent* Mesh = NewPlayer->GetMesh())
		{
			Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
	}

	// Apply cached move_speed immediately so the player has the correct MaxWalkSpeed
	// from frame 1. stats_update packets arrive before the actor is spawned (world travel),
	// so the authoritative speed sits in PlayerStatsManager cache — apply it now.
	if (bIsLocal && PlayerStatsManager)
	{
		FStatAttributeEntry SpeedAttr;
		if (PlayerStatsManager->GetAttribute(TEXT("move_speed"), SpeedAttr) && SpeedAttr.effective > 0.f)
		{
			NewPlayer->ApplyServerMoveSpeed(SpeedAttr.effective);
			UE_LOG(LogTemp, Log, TEXT("SpawnPlayerForClient: Applied cached move_speed=%.1f to CharID=%d"),
				SpeedAttr.effective, PlayerData.characterData.characterId);
		}
	}

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
		UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 3b. Calling PC->Possess(Player) � camera will switch NOW"));
		PC->Possess(Player);
		{
			FRotator CR = PC->GetControlRotation();
			FVector  CamLoc = Player->GetFollowCamera() ? Player->GetFollowCamera()->GetComponentLocation() : FVector::ZeroVector;
			UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] 3c. PC->Possess done. Pawn=%s ControlRot=(P=%.1f Y=%.1f R=%.1f) CameraLoc=(%.0f,%.0f,%.0f)"),
				PC->GetPawn() ? *PC->GetPawn()->GetName() : TEXT("NULL"),
				CR.Pitch, CR.Yaw, CR.Roll,
				CamLoc.X, CamLoc.Y, CamLoc.Z);
		}
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

			// Bind cosmetic visual component to remote equipment updates so that
			// helmets correctly hide hair for other players on the same screen.
			if (UCosmeticVisualComponent* CosmeticVis = NewPlayer->GetCosmeticVisualComponent())
			{
				CosmeticVis->SetOwnerCharacterId(RemoteCharId);
				EquipmentManager->OnRemoteEquipmentStateReceivedDelegate.AddDynamic(
					CosmeticVis, &UCosmeticVisualComponent::HandleRemoteEquipmentState);
			}

			// Replay any PLAYER_EQUIPMENT_UPDATE that arrived before this actor was spawned.
			// This resolves the race where the server broadcasts equipment immediately after
			// playerReady but the client is still deferred in PendingRemotePlayerSpawns.
			if (const FEquipmentStateData* Cached = EquipmentManager->GetCachedRemoteEquipmentState(RemoteCharId))
			{
				if (Cached->slots.Num() > 0)
				{
					VisComp->HandleRemoteEquipmentState(*Cached);
					UE_LOG(LogTemp, Log, TEXT("SpawnPlayerForClient: Replayed cached equipment for remote CharID=%d (%d slot(s))"),
						RemoteCharId, Cached->slots.Num());

					// Also replay to cosmetics so hair-hide is applied from the cached state.
					if (UCosmeticVisualComponent* CosmeticVis = NewPlayer->GetCosmeticVisualComponent())
					{
						CosmeticVis->HandleRemoteEquipmentState(*Cached);
					}
				}
			}

			// NOTE: Do NOT call RequestGetEquipment(RemoteCharId) here — the server
			// resolves getEquipment requests by session character ID (i.e. the LOCAL
			// player's character), so the request would return OUR OWN equipment, not
			// the remote player's.  The server now broadcasts the new player's equipment
			// to all existing clients via broadcastEquipmentUpdate inside
			// handlePlayerReadyEvent, so the PLAYER_EQUIPMENT_UPDATE will arrive
			// automatically and be routed here through OnRemoteEquipmentStateReceivedDelegate.
		}

		// Apply initial equipped title to this remote player's nameplate.
		// The server includes equippedTitleDisplayName (and equippedTitleSlug) in
		// joinGameCharacter/getConnectedCharacters.
		if (!PlayerData.characterData.equippedTitleSlug.IsEmpty())
		{
			const FString& TitleText = PlayerData.characterData.equippedTitleDisplayName.IsEmpty()
				? PlayerData.characterData.equippedTitleSlug
				: PlayerData.characterData.equippedTitleDisplayName;
			NewPlayer->SetEquippedTitle(TitleText);
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

	if (bJoinGameInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("JoinSelectedCharacterToGame: join already in progress, ignoring duplicate call (charId=%d)"), CurrentCharacterID);
		return;
	}

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

	bJoinGameInProgress = true;

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
		bJoinGameInProgress = false;
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

				// Unsubscribe cosmetic visual component from the same remote delegate.
				UCosmeticVisualComponent* CosmeticVis = PlayerToRemove->GetCosmeticVisualComponent();
				if (CosmeticVis && EquipmentManager)
				{
					EquipmentManager->OnRemoteEquipmentStateReceivedDelegate.RemoveDynamic(
						CosmeticVis, &UCosmeticVisualComponent::HandleRemoteEquipmentState);
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

			// Bug 10 fix: if the remote player is still marked dead but is now sending
			// movement updates, they must have respawned on the server side.
			// Auto-revive them so their dead animation and disabled movement are cleared.
			// (respawnResult is sent only to the respawning player's client; other clients
			// detect the respawn here when position packets resume after death.)
			if (PlayerToMove->GetIsDead())
			{
				PlayerToMove->SetDead_Implementation(false);
				// stats_update after respawn is unicast to the respawning player only,
				// so other clients cannot know the exact new HP.  Restore to max as a
				// best-effort approximation — the nameplate will be corrected if a
				// subsequent charAttributesUpdate or stats broadcast arrives.
				const int32 MaxHP = PlayerToMove->GetMaxHealth_Implementation();
				PlayerToMove->SetPlayerCurrentHPPoints(MaxHP);
				UE_LOG(LogTemp, Log, TEXT("MovePlayerForClient: Auto-revived remote player CharID=%d HP set to %d"),
					clientData.characterData.characterId, MaxHP);
			}
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
		AudioManager->Init(this, AudioConfig);
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

USkillShopManager* UMyGameInstance::GetSkillShopManager() const
{
	return SkillShopManager;
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

void UMyGameInstance::RouteEmoteActionToPlayer(int32 CharacterId, const FString& EmoteSlug, const FString& AnimationName)
{
    ABasicPlayer* TargetPlayer = GetPlayerByCharacterId(CharacterId);
    if (TargetPlayer)
    {
        TargetPlayer->PlayEmoteForCharacter(EmoteSlug, AnimationName);
    }
}
