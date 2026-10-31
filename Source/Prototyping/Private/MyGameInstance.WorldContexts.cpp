// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Authentication/AuthenticationManager.h"
#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Teleport/TeleportManager.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Mobs/SpawnZoneManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Audio/AudioManager.h"
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
#include "Engine/World.h"
#include "NavigationSystem.h"


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
	if (!bTransitioningToGameWorld && !bReturningToLogin)
	{
		UE_LOG(LogConnection, Verbose, TEXT("InvalidateManagerWorldContexts: SKIPPED (not transitioning - PreLoadMap from another PIE instance)"));
		return;
	}

	UE_LOG(LogConnection, Warning, TEXT("InvalidateManagerWorldContexts: Nulling world context on all managers before map load"));

	// Set worldContext to nullptr on every manager that holds one.
	// This prevents stale TObjectPtr<UWorld> handle assertions (0xFFFFFFFFFFFFFFFF)
	// when network packets arrive while the old world is being destroyed.
	if (NetworkManager)      { NetworkManager->SetWorldContext(nullptr); }
	if (PingManager)         { PingManager->SetWorldContext(nullptr); }
	if (AuthenticationManager) { AuthenticationManager->SetWorldContext(nullptr); }
	if (PlayerManager)       { PlayerManager->SetWorldContext(nullptr); }
	if (MOBManager)          { MOBManager->SetWorldContext(nullptr); MOBManager->ClearWorldState(); }
	if (SpawnZoneManager)    { SpawnZoneManager->SetWorldContext(nullptr); SpawnZoneManager->ClearWorldState(); }
	if (ItemManager)         { ItemManager->SetWorldContext(nullptr); ItemManager->ClearWorldState(); }
	if (InventoryManager)    { InventoryManager->SetWorldContext(nullptr); }
	if (HarvestManager)      { HarvestManager->SetWorldContext(nullptr); }
	if (CombatSystemManager) { CombatSystemManager->SetWorldContext(nullptr); }
	if (NPCManager)          { NPCManager->SetWorldContext(nullptr); NPCManager->ClearWorldState(); }
	if (WorldObjectManager)  { WorldObjectManager->SetWorldContext(nullptr); WorldObjectManager->ClearWorldState(); }
	if (TimeSyncService)     { TimeSyncService->SetWorldContext(nullptr); }
	// Snapshot active cooldowns in old-world seconds BEFORE the world goes
	// away; the new world's clock restarts near zero, so raw endTimes would
	// read as permanently on cooldown. Re-anchored in RefreshManagerWorldContexts.
	if (PlayerSkillManager)  { PlayerSkillManager->BeginTravelCooldownSnapshot(); }
	// Release audio components that belong to the dying world so CrossfadeToTrack
	// re-creates them in the new world instead of silently replaying on a dead device.
	if (AudioManager)        { AudioManager->InvalidateAudioComponents(); }
}

void UMyGameInstance::QuiesceNavMeshBuilds(UWorld* World)
{
	if (!IsValid(World)) return;

	// Only our own world: never stall on another PIE instance's navmesh
	// (each client has its own GameInstance + world in this process).
	UWorld* MyWorld = GetWorld();
	if (!IsValid(MyWorld) || MyWorld != World)
	{
		return;
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		UE_LOG(LogConnection, Warning, TEXT("QuiesceNavMeshBuilds: cancelling async navmesh builds for world '%s'"),
			*World->GetName());
		NavSys->CancelBuild();
	}
}

void UMyGameInstance::RefreshManagerWorldContexts()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogGameplay, Error, TEXT("RefreshManagerWorldContexts: World is null"));
		return;
	}

	UE_LOG(LogConnection, Warning, TEXT("RefreshManagerWorldContexts: Refreshing world context on all managers"));

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
	if (TeleportManager) { TeleportManager->SetWorldContext(World); }
	if (MOBManager) { MOBManager->SetWorldContext(World); }
	if (SpawnZoneManager) { SpawnZoneManager->SetWorldContext(World); }
	if (ItemManager) { ItemManager->SetWorldContext(World); }
	if (InventoryManager) { InventoryManager->SetWorldContext(World); }
	if (HarvestManager) { HarvestManager->SetWorldContext(World); }
	if (CombatSystemManager) { CombatSystemManager->SetWorldContext(World); }
	if (NPCManager) { NPCManager->SetWorldContext(World); }
	if (WorldObjectManager) { WorldObjectManager->SetWorldContext(World); }
	if (TimeSyncService) { TimeSyncService->SetWorldContext(World); }
	// Re-anchor travel-persisted cooldowns to the new world's clock (it
	// restarted near zero; raw old-world endTimes would read as stuck).
	if (PlayerSkillManager) { PlayerSkillManager->ApplyTravelCooldownSnapshot(); }

	// Re-push the SoundMix and re-apply all cached volume overrides.
	// The audio device loses its SoundMix modifier stack on world teardown,
	// so without this call every volume slider becomes a no-op after a level transition.
	if (AudioManager) { AudioManager->ReapplySoundMix(); }
}

void UMyGameInstance::ProcessPendingSpawns()
{
	UE_LOG(LogConnection, Verbose, TEXT("ProcessPendingSpawns: PendingSpawnClientId=%d, PendingRemote=%d"),
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

