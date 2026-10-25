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

void UMyGameInstance::InitGameSystems()
{
	// Initialize the AudioManager
	AudioManager = NewObject<UAudioManager>(this);
	if (AudioManager)
	{
		AudioManager->Init(this, AudioConfig);
		UE_LOG(LogGameplay, Verbose, TEXT("AudioManager created"));
	}

	// Initialize the item manager with the visuals data table if available
	if (ItemManager && ItemVisualsDataTable)
	{
		ItemManager->LoadItemVisualsDataTable(ItemVisualsDataTable);
		UE_LOG(LogGameplay, Verbose, TEXT("Initialized ItemManager with configured ItemVisualsDataTable"));
	}
	else if (ItemManager)
	{
		UE_LOG(LogGameplay, Warning, TEXT("ItemVisualsDataTable not configured in editor - item visuals will use defaults"));
	}
}

void UMyGameInstance::SetInventoryManager(UInventoryManager* NewInventoryManager)
{
	if (NewInventoryManager)
	{
		InventoryManager = NewInventoryManager;
		UE_LOG(LogGameplay, Warning, TEXT("MyGameInstance: Inventory Manager reference set"));
	}
	else
	{
		UE_LOG(LogGameplay, Warning, TEXT("MyGameInstance: Cannot set null Inventory Manager"));
	}
}

UTimeSyncService* UMyGameInstance::GetTimeSyncService()
{
	return TimeSyncService;
}

UPingManager* UMyGameInstance::GetPingManager()
{
	return PingManager;
}

void UMyGameInstance::ProcessTimeSyncData(const FMessageDataStruct& MessageData)
{
	// Process time sync data from server response
	if (TimeSyncService && !MessageData.timestamp.IsEmpty() && MessageData.serverRecvMs > 0 && MessageData.serverSendMs > 0)
	{
		// Use timestamp as request ID since we don't have a proper request ID in the current structure
		FString RequestId = MessageData.timestamp;
		TimeSyncService->UpdateTimeSyncData(RequestId, MessageData.serverRecvMs, MessageData.serverSendMs);
		
		UE_LOG(LogGameplay, Warning, TEXT("MyGameInstance: Updated time sync data - RequestId: %s, ServerRecv: %lld, ServerSend: %lld"),
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

void UMyGameInstance::OnApplicationReactivated()
{
    // When the window regains focus, the world FTimerManager resumes.
    // Force a fresh ping + time-sync so skills don't get stuck on stale
    // server-epoch cooldowns from IsTimeSyncValid() having dropped out.
    if (PingManager)
    {
        PingManager->RestartPingUpdates();
    }
    if (TimeSyncService)
    {
        TimeSyncService->RequestTimeSync();
    }

    // Defence in depth: immediately convert any server-clock cooldowns to
    // world-time so skills don't stay stuck even before time-sync recovers.
    if (UPlayerSkillManager* SkillMgr = GetPlayerSkillManager())
    {
        SkillMgr->ConvertServerClockCooldownsToWorldTime();
    }

    UE_LOG(LogGameplay, Verbose, TEXT("GameInstance: Application reactivated — ping + time sync + cooldown conversion done"));
}
