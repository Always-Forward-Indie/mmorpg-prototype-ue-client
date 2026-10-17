// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Components/ArrowComponent.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "DevMode/DevModeDataProvider.h"
#include "DevMode/DevModeConsoleCommands.h"
#include "DevMode/LiveBotDriver.h"
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
#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Animation/AnimNotify_Footstep.h"
#include "Engine/StreamableManager.h"
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

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"


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
		// Net subscription is deferred to BasicPlayer::BeginPlay() where
		// OwnerCharacterId is set — prevents processing unowned packets.
	}

	if (HarvestManager != nullptr) {
		// set WorldContext
		HarvestManager->SetWorldContext(GetWorld());
		// set game instance
		HarvestManager->SetGameInstance(this);
		// Initialize the harvest manager
		HarvestManager->Initialize(NetworkManager);
		// Net subscription is deferred to BasicPlayer::BeginPlay() where
		// the local character is available — prevents double-subscription.
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

	// Preload all footstep sounds from DT_FootstepsSounds to eliminate LoadSynchronous hitches
	if (FootstepSoundsTable)
	{
		TArray<FFootstepSoundData*> AllRows;
		FootstepSoundsTable->GetAllRows<FFootstepSoundData>(TEXT("PreloadFootstep"), AllRows);
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		for (const FFootstepSoundData* Row : AllRows)
		{
			if (!Row) continue;
			for (const TSoftObjectPtr<USoundBase>& SoundSoft : Row->FootstepSounds)
			{
				if (!SoundSoft.IsNull())
				{
					Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath());
				}
			}
			if (!Row->FootstepVFX.IsNull())
			{
				Streamable.RequestAsyncLoad(Row->FootstepVFX.ToSoftObjectPath());
			}
			if (!Row->DefaultAttenuation.IsNull())
			{
				Streamable.RequestAsyncLoad(Row->DefaultAttenuation.ToSoftObjectPath());
			}
		}
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

// Live bot driver (DEV-ONLY): auto-login + join for -bot-live=<scenario>.
// No-op unless the flag was passed — normal online flow is untouched.
