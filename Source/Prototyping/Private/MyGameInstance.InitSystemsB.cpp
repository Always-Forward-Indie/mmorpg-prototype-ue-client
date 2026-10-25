// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Prototyping.h"
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


void UMyGameInstance::InitSocial()
{
	// Initialize Dialogue System
	if (DialogueManager)
	{
		DialogueManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogGameplay, Warning, TEXT("DialogueManager initialized"));
	}

	if (DialogueNetworkHandler && DialogueManager)
	{
		DialogueNetworkHandler->Initialize(DialogueManager, GetNetworkManager());
		DialogueNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("DialogueNetworkHandler initialized and subscribed"));
	}

	// Initialize Quest System
	if (QuestManager)
	{
		QuestManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogGameplay, Warning, TEXT("QuestManager initialized"));
	}

	if (QuestNetworkHandler && QuestManager)
	{
		QuestNetworkHandler->Initialize(QuestManager, GetNetworkManager());
		QuestNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("QuestNetworkHandler initialized and subscribed"));
	}

}

void UMyGameInstance::InitEconomy()
{
	// Initialize Equipment System
	if (EquipmentManager)
	{
		EquipmentManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogGameplay, Warning, TEXT("EquipmentManager initialized"));
	}

	if (EquipmentNetworkHandler && EquipmentManager)
	{
		EquipmentNetworkHandler->Initialize(EquipmentManager, GetNetworkManager());
		EquipmentNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("EquipmentNetworkHandler initialized and subscribed"));
	}

	// Initialize Vendor System
	if (VendorManager)
	{
		VendorManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogGameplay, Warning, TEXT("VendorManager initialized"));
	}

	if (VendorNetworkHandler && VendorManager)
	{
		VendorNetworkHandler->Initialize(VendorManager, GetNetworkManager());
		VendorNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("VendorNetworkHandler initialized and subscribed"));
	}

	// Initialize Repair System
	if (RepairManager)
	{
		RepairManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogGameplay, Warning, TEXT("RepairManager initialized"));
	}

	if (RepairNetworkHandler && RepairManager)
	{
		RepairNetworkHandler->Initialize(RepairManager, GetNetworkManager());
		RepairNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("RepairNetworkHandler initialized and subscribed"));
	}

	// Initialize Skill Shop System (NPC trainer)
	// Must be after PlayerSkillManager is created by PlayerSkillSystemFactory
	if (SkillShopManager)
	{
		SkillShopManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogGameplay, Warning, TEXT("SkillShopManager initialized"));
	}

	if (SkillShopNetworkHandler && SkillShopManager)
	{
		SkillShopNetworkHandler->Initialize(SkillShopManager, GetNetworkManager(), PlayerSkillManager);
		SkillShopNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("SkillShopNetworkHandler initialized and subscribed"));
	}

	// Initialize Trade System
	if (TradeManager)
	{
		TradeManager->Initialize(GetNetworkManager(), this);
		UE_LOG(LogGameplay, Warning, TEXT("TradeManager initialized"));
	}

	if (TradeNetworkHandler && TradeManager)
	{
		TradeNetworkHandler->Initialize(TradeManager, GetNetworkManager());
		TradeNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("TradeNetworkHandler initialized and subscribed"));
	}

	// Initialize Bestiary System
	if (BestiaryNetworkHandler)
	{
		BestiaryNetworkHandler->Initialize(this, GetNetworkManager());
		BestiaryNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("BestiaryNetworkHandler initialized and subscribed"));
	}

}

void UMyGameInstance::InitCharacterNets()
{
	// Initialize PlayerStats network handler
	if (PlayerStatsNetworkHandler && PlayerStatsManager)
	{
		PlayerStatsNetworkHandler->Initialize(PlayerStatsManager, GetNetworkManager(), this);
		PlayerStatsNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("PlayerStatsNetworkHandler initialized and subscribed"));
	}

	// Initialize Mastery system
	if (MasteryManager && MasteryNetworkHandler)
	{
		MasteryNetworkHandler->Initialize(MasteryManager, GetNetworkManager(), this);
		MasteryNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("MasteryNetworkHandler initialized and subscribed"));
	}

	// Initialize Reputation system
	if (ReputationManager && ReputationNetworkHandler)
	{
		ReputationNetworkHandler->Initialize(ReputationManager, GetNetworkManager(), this);
		ReputationNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("ReputationNetworkHandler initialized and subscribed"));
	}

	// Initialize Titles system
	if (TitleManager && TitleNetworkHandler)
	{
		TitleNetworkHandler->Initialize(TitleManager, GetNetworkManager(), this);
		TitleNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("TitleNetworkHandler initialized and subscribed"));
	}

	// Initialize Emote system
	if (EmoteManager && EmoteNetworkHandler)
	{
		EmoteNetworkHandler->Initialize(EmoteManager, GetNetworkManager(), this);
		EmoteNetworkHandler->SubscribeToNetworkEvents();

		// Route emoteAction broadcasts to the correct ABasicPlayer in the world
		EmoteManager->OnEmoteActionReceived.AddDynamic(this, &UMyGameInstance::RouteEmoteActionToPlayer);

		UE_LOG(LogGameplay, Warning, TEXT("EmoteNetworkHandler initialized and subscribed"));
	}

	// Initialize NPC Ambient Speech system
	if (AmbientSpeechManager && AmbientSpeechNetworkHandler)
	{
		AmbientSpeechManager->Initialize(this);
		AmbientSpeechNetworkHandler->Initialize(AmbientSpeechManager, GetNetworkManager());
		AmbientSpeechNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("AmbientSpeechNetworkHandler initialized and subscribed"));
	}

}

void UMyGameInstance::InitWorldObjects()
{
	// Initialize World Interactive Objects system
	if (WorldObjectManager)
	{
		WorldObjectManager->SetWorldContext(GetWorld());
		WorldObjectManager->Initialize(GetNetworkManager(), this);

		if (WIODefinitionTable)
		{
			WorldObjectManager->SetDefinitionTable(WIODefinitionTable);
			UE_LOG(LogGameplay, Warning, TEXT("WorldObjectManager: WIODefinitionTable assigned from GameInstance"));
		}
		if (WIODefaultActorClass)
		{
			WorldObjectManager->DefaultActorClass = WIODefaultActorClass;
			UE_LOG(LogGameplay, Warning, TEXT("WorldObjectManager: WIODefaultActorClass assigned from GameInstance"));
		}

		UE_LOG(LogGameplay, Warning, TEXT("WorldObjectManager initialized"));
	}

	if (WIONetworkHandler && WorldObjectManager)
	{
		WIONetworkHandler->Initialize(WorldObjectManager, GetNetworkManager());
		WIONetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("WIONetworkHandler initialized and subscribed"));
	}

}

void UMyGameInstance::InitChatSystems()
{
	// Initialize Chat System
	if (ChatManager)
	{
		ChatManager->Initialize(this, GetNetworkManager());
		UE_LOG(LogGameplay, Warning, TEXT("ChatManager initialized"));
	}

	if (ChatNetworkHandler && ChatManager)
	{
		ChatNetworkHandler->Initialize(ChatManager, GetNetworkManager());
		ChatNetworkHandler->SubscribeToNetworkEvents();
		UE_LOG(LogGameplay, Warning, TEXT("ChatNetworkHandler initialized and subscribed"));
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
