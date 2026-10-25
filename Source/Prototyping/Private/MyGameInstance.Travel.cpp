// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Gameplay/Players/MyCameraActor.h"
#include "Authentication/AuthenticationManager.h"
#include "Gameplay/Players/PlayerManager.h"
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


// load LoginLevel
void UMyGameInstance::LoadLoginLevel()
{
	// Reset join flow guard so the player can select a character again after returning to login
	JoinGuard.Reset();

	if (DevModeConfig.bEnabled)
	{
		// DevMode: skip login, go straight to the configured level.
		// LevelOverride takes priority; fall back to DebugLevelName when not set.
		LevelBeingLoaded = (DevModeConfig.LevelOverride != NAME_None)
			? DevModeConfig.LevelOverride
			: DebugLevelName;
	}
	else if (bDebug)
	{
		LevelBeingLoaded = DebugLevelName;
	}
	else
	{
		LevelBeingLoaded = LoginLevelName;
	}

	UE_LOG(LogConnection, Verbose, TEXT("LoadLoginLevel: Opening %s via OpenLevel"), *LevelBeingLoaded.ToString());
	// Travel measurement (see TransitionToGameWorld): reported on PostLoadMap.
	LoadTravelStartTime = FPlatformTime::Seconds();
	LoadTravelStartFrame = GFrameCounter;
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Travel START: map '%s' (t=%.1fs, frame %llu)"),
		*LevelBeingLoaded.ToString(), LoadTravelStartTime, LoadTravelStartFrame);
	// Setup MoviePlayer loading screen before OpenLevel — widget survives CleanupWorld.
	SetupMoviePlayerLoadingScreen();
	// Arm the focus keeper before the load: staggered launches steal focus
	// mid-load and the throttled window crawls (measured ~3-10 fps unfocused).
	StartWindowFocusKeeper();
	UGameplayStatics::OpenLevel(GetWorld(), LevelBeingLoaded, true);
	// PostLoadMapWithWorld → OnPostLoginLevelLoaded will handle initialization
}

void UMyGameInstance::ReturnToLoginLevel()
{
	if (bReturningToLogin)
	{
		UE_LOG(LogConnection, Warning, TEXT("ReturnToLoginLevel: Already in progress, ignoring duplicate call"));
		return;
	}

	UE_LOG(LogConnection, Verbose, TEXT("ReturnToLoginLevel: Disconnecting from servers and returning to login"));

	// Stop idle tracking — unregisters input processor and clears timers
	// before we tear down the game world and network connections.
	if (IdleTimeoutManager)
	{
		IdleTimeoutManager->StopTracking();
	}

	// 1. Send disconnect packets to Login Server and Chunk Server.
	//    The server session is terminated; the client will re-authenticate when
	//    logging in again. Skipped in DevMode offline (never connected).
	if (ClientData.clientId > 0 && !ClientData.hash.IsEmpty() && !DevModeConfig.bEnabled)
	{
		if (AuthenticationManager)
		{
			AuthenticationManager->SendLeaveGameRequest(ClientData);
		}
		if (PlayerManager)
		{
			PlayerManager->SendLeaveGameRequest(ClientData);
		}
	}

	// 2. Tear down all network connections WITHOUT setting bIsShutDown.
	//    Disconnect() closes sockets and stops worker threads, but leaves the
	//    NetworkManager in a state where ConnectLoginServer() can recreate them.
	if (NetworkManager)
	{
		NetworkManager->Disconnect();

		// Clear skill data so cooldowns from the previous session don't
		// persist across relogin and get stuck on server-epoch timestamps.
		if (PlayerSkillManager)
		{
			PlayerSkillManager->InitializePlayerSkills(FPlayerSkillsInitializationData());
		}
	}

	// 3. Reset game-world state so the next login starts from a clean slate.
	JoinGuard.Reset();
	bGameWorldReady = false;
	bTransitioningToGameWorld = false;
	ReadyFlags = 0;
	ConnectedPlayers.Empty();
	SpawnedPlayers.Empty();
	Player = nullptr;

	// 4. Return to login level via OpenLevel.
	//    MoviePlayer widget survives CleanupWorld → no black screen between worlds.
	SetupMoviePlayerLoadingScreen();
	bReturningToLogin = true;
	LevelBeingLoaded = LoginLevelName;

	// Defer OpenLevel by one tick so Slate composites the loading screen first.
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float) -> bool
		{
			UGameplayStatics::OpenLevel(GetWorld(), LoginLevelName, true);
			return false;
		}),
		0.0f);
}

// Defined here (single TU): process-global World Partition transition slot,
// shared via extern in MyGameInstance.h. Both PIE instances run in the same
// process so a process-global is required here — never make it a member.
volatile int32 GActiveWorldPartitionTransitions = 0;


void UMyGameInstance::LoadLevel(const FName& LevelName)
{
	LevelBeingLoaded = LevelName;
	UE_LOG(LogConnection, Verbose, TEXT("LoadLevel: Opening %s via OpenLevel"), *LevelName.ToString());
	SetupMoviePlayerLoadingScreen();

	// Defer OpenLevel by one tick so Slate composites the loading screen first.
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this, LevelName](float) -> bool
		{
			UGameplayStatics::OpenLevel(GetWorld(), LevelName, true);
			return false;
		}),
		0.0f);
}

void UMyGameInstance::ForceGameWindowToFront()
{
	if (!FSlateApplication::IsInitialized()) { return; }
	if (TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow())
	{
		if (TSharedPtr<FGenericWindow> NativeWindow = ActiveWindow->GetNativeWindow())
		{
			NativeWindow->BringToFront();
			NativeWindow->SetWindowFocus();
		}
	}
}

void UMyGameInstance::StartWindowFocusKeeper()
{
	// Focus keeper: while the loading screen is up, periodically take
	// foreground back. A subsequently launched client steals OS focus mid-load
	// (1s stagger); the throttled window crawls at a fraction of its tick rate
	// and every gate/timer/frame-counter dilates with it. Self-stops once the
	// screen is gone or after 8s.
	const double FocusNudgeStartTime = FPlatformTime::Seconds();
	FTSTicker::GetCoreTicker().RemoveTicker(WindowFocusNudgeTickerHandle);
	WindowFocusNudgeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this, FocusNudgeStartTime](float) -> bool
		{
			if (!LoadingScreenWidget) { return false; }
			if (FPlatformTime::Seconds() - FocusNudgeStartTime > 8.0)
			{
				UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] Focus keeper: giving up after 8s (screen still up)"));
				return false;
			}
			ForceGameWindowToFront();
			return true;
		}),
		0.5f);
}

void UMyGameInstance::TransitionToGameWorld()
{
	if (GameWorldMap.IsNull())
	{
		UE_LOG(LogConnection, Error, TEXT("TransitionToGameWorld: GameWorldMap is not set! Assign it in the GameInstance Blueprint defaults."));
		return;
	}

	if (bTransitioningToGameWorld)
	{
		UE_LOG(LogConnection, Warning, TEXT("TransitionToGameWorld: Already transitioning, ignoring duplicate call"));
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

	UE_LOG(LogConnection, Warning, TEXT("TransitionToGameWorld: Starting transition to map '%s' (from path '%s')"), 
		*MapName, *MapAssetPath);

	// Track which level we're opening so PostLoadMapWithWorld can route to the correct handler.
	// Must be set before DoOpenLevel so OnPostLoginLevelLoaded sees it.
	LevelBeingLoaded = FName(*MapName);

	bTransitioningToGameWorld = true;

	// Travel measurement: reported when PostLoadMap arrives (see
	// OnPostLoginLevelLoaded). If travel crawls (throttled present / RHI fence
	// waits on an unfocused window), the numbers show it instead of "hangs".
	LoadTravelStartTime = FPlatformTime::Seconds();
	LoadTravelStartFrame = GFrameCounter;
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Travel START: map '%s' (t=%.1fs, frame %llu)"),
		*MapName, LoadTravelStartTime, LoadTravelStartFrame);

	// Force the game window to foreground BEFORE the level transition so the
	// FTSTicker poller (CheckGameWorldReady) and render-thread frame counter
	// (StartFrameCountdown) run at full speed. Foreground is re-asserted after
	// the load completes (see OnPostLoginLevelLoaded) because staggered client
	// launches steal focus mid-load.
	ForceGameWindowToFront();
	// Arm the keeper BEFORE the load: the crawl happens during travel, and a
	// single pre-load BringToFront does not survive the next client's launch.
	StartWindowFocusKeeper();

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
	// MoviePlayer widget survives CleanupWorld → no black/skybox gap.
	SetupMoviePlayerLoadingScreen();

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

	// Defer DoOpenLevel by one tick so Slate has a full frame to composite the
	// loading screen before the world begins to tear down.
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float) -> bool
		{
			DoOpenLevel();
			return false;
		}),
		0.0f);
}

void UMyGameInstance::DoOpenLevel()
{
	UWorld* CurrentWorld = GetWorld();
	if (!CurrentWorld)
	{
		UE_LOG(LogConnection, Error, TEXT("DoOpenLevel: GetWorld() returned nullptr"));
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
		UE_LOG(LogConnection, Warning,
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
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 1. OpenLevel('%s') called"), *TravelPath);

	// Ticker is started in OnPostLoginLevelLoaded after the new world is
	// loaded — prevents the ticker from firing on the old world mid-transition.
}

// PostLoadMapWithWorld handler — fires after every OpenLevel completes.
// Replaces the old OnLoginLevelLoaded streaming callback with a unified handler
