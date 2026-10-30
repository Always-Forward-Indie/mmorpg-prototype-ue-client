// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Gameplay/Players/MyCameraActor.h"
#include "Audio/AudioManager.h"
#include "Authentication/AuthenticationManager.h"
#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Mobs/SpawnZoneManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "MoviePlayer.h"
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
#include "Utils/NetCore/TravelGate.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"


// for Login, DevMode/Debug, and return-to-login.
void UMyGameInstance::OnPostLoginLevelLoaded(UWorld* LoadedWorld)
{
	if (!LoadedWorld) return;

	UE_LOG(LogConnection, Warning, TEXT("OnPostLoginLevelLoaded: %s"), *LevelBeingLoaded.ToString());

	// Travel measurement: how long the travel took in wall-clock and frames.
	// A crawl here (seconds high, frames low) means the engine travel itself
	// was throttled (unfocused window / contended GPU) — everything downstream
	// (gates, flags, safeties) can only start after this point.
	if (LoadTravelStartTime > 0.0)
	{
		const double TravelElapsed = FPlatformTime::Seconds() - LoadTravelStartTime;
		const uint64 TravelFrames = GFrameCounter - LoadTravelStartFrame;
		const double TravelFps = TravelElapsed > 0.0 ? (double)TravelFrames / TravelElapsed : 0.0;
		if (FTravelGate::HasTravelStalled(TravelElapsed, TravelFrames))
		{
			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Travel DONE THROTTLED: map '%s' took %.1fs wall-clock (%llu frames, avg %.1f fps) — engine travel starved (unfocused window / RHI fence)"),
				*LevelBeingLoaded.ToString(), TravelElapsed, TravelFrames, TravelFps);
		}
		else
		{
			UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] Travel DONE: map '%s' took %.1fs wall-clock (%llu frames, avg %.1f fps)"),
				*LevelBeingLoaded.ToString(), TravelElapsed, TravelFrames, TravelFps);
		}
		LoadTravelStartTime = 0.0;
	}

	// ---- Login Level ----
	if (LevelBeingLoaded == LoginLevelName)
	{
		// Guard: if we're in the middle of transitioning to game world, do NOT
		// add login widgets.  This prevents login UI from flashing on top of
		// the game world loading screen when PostLoadMapWithWorld fires for an
		// unexpected intermediate map.
		if (bTransitioningToGameWorld)
		{
			UE_LOG(LogConnection, Warning, TEXT("OnPostLoginLevelLoaded: LoginLevel match but bTransitioningToGameWorld=true — skipping login UI"));
			return;
		}

		AddLoginWidgetToViewport();
		AddMonitorStatsWidgetToViewport();

		if (LoadedWorld->GetFirstPlayerController())
		{
			LoadedWorld->GetFirstPlayerController()->bShowMouseCursor = true;
		}

		// Re-apply custom cursor shape (do NOT steal focus — login UI has text fields)
		if (FSlateApplication::IsInitialized())
		{
			if (TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor())
			{
				if (PreloadedDefaultCursorHandle)
				{
					PlatformCursor->SetTypeShape(EMouseCursor::Default, PreloadedDefaultCursorHandle);
				}
			}
		}

		// Read camera/spawn transforms from LoginLevelSetupActor
		ALoginLevelSetupActor* SetupActor = Cast<ALoginLevelSetupActor>(
			UGameplayStatics::GetActorOfClass(LoadedWorld, ALoginLevelSetupActor::StaticClass()));
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
			if (SetupActor->PodiumSlots.Num() > 0 && SetupActor->PodiumSlots[0])
			{
				PodiumSpawnRotation = SetupActor->PodiumSlots[0]->GetComponentRotation();
			}

			UE_LOG(LogConnection, Verbose, TEXT("LoginLevelSetupActor found — camera and spawn transforms applied."));
		}
		else
		{
			UE_LOG(LogConnection, Warning, TEXT("LoginLevelSetupActor not found in Login Level — using Blueprint-configured values."));
		}

		// Spawn login camera (idempotent: PostLoadMap can fire twice for the
		// login map — reuse the live camera instead of stacking actors).
		if (!IsValid(LoginLevelCamera) || LoginLevelCamera->GetWorld() != LoadedWorld)
		{
			LoginLevelCamera = LoadedWorld->SpawnActor<AMyCameraActor>(LoginCameraClass, LoginLevelCameraLocation, LoginLevelCameraRotation);
		}
		if (LoginLevelCamera)
		{
			if (LoadedWorld->GetFirstPlayerController())
			{
				LoadedWorld->GetFirstPlayerController()->SetViewTargetWithBlend(LoginLevelCamera, 0.0f);

				if (AudioManager) { AudioManager->ReapplySoundMix(); }

				if (AudioManager && !AudioManager->LoginPlaylistId.IsEmpty())
				{
					AudioManager->PlayPlaylist(AudioManager->LoginPlaylistId, true);
				}
				else
				{
					LoginLevelCamera->PlaySound(LoginMusicSoundSource);
				}
			}
		}

		// Network init: return path vs first load
		if (bReturningToLogin)
		{
			// Return from game world — restore world contexts and reconnect
			// without re-subscribing delegates (already registered in first InitNetworkingSetup)
			if (NetworkManager)
			{
				NetworkManager->SetWorldContext(LoadedWorld);
				NetworkManager->SetMessageBoxPopupClass(MessageBoxPopupClass);
				NetworkManager->ConnectLoginServer();
				NetworkManager->ConnectGameServer();
				NetworkManager->ConnectChunkServer();
				NetworkManager->StartPollingLoginServer();
				NetworkManager->StartPollingGameServer();
				NetworkManager->StartPollingChunkServer();
			}

			if (PingManager)         { PingManager->SetWorldContext(LoadedWorld); PingManager->RestartPingUpdates(); }
			if (AuthenticationManager) { AuthenticationManager->SetWorldContext(LoadedWorld); }
			if (PlayerManager)       { PlayerManager->SetWorldContext(LoadedWorld); }
			if (MOBManager)          { MOBManager->SetWorldContext(LoadedWorld); MOBManager->ClearWorldState(); }
			if (SpawnZoneManager)    { SpawnZoneManager->SetWorldContext(LoadedWorld); SpawnZoneManager->ClearWorldState(); }
			if (ItemManager)         { ItemManager->SetWorldContext(LoadedWorld); }
			if (InventoryManager)    { InventoryManager->SetWorldContext(LoadedWorld); }
			if (HarvestManager)      { HarvestManager->SetWorldContext(LoadedWorld); }
			if (CombatSystemManager) { CombatSystemManager->SetWorldContext(LoadedWorld); }
			if (NPCManager)          { NPCManager->SetWorldContext(LoadedWorld); }
			if (WorldObjectManager)  { WorldObjectManager->SetWorldContext(LoadedWorld); WorldObjectManager->ClearWorldState(); }
			if (TimeSyncService)     { TimeSyncService->SetWorldContext(LoadedWorld); }
			if (AudioManager)        { AudioManager->ReapplySoundMix(); }

			AddMonitorStatsWidgetToViewport();
			if (PingManager && IsValid(MonitorStatsWidget))
			{
				PingManager->Initialize(NetworkManager, MonitorStatsWidget);
			}

			bReturningToLogin = false;
		}
		else
		{
			// First load — full network init with delegate subscriptions
			InitNetworkingSetup();
		}

		// Initialize CharacterPreviewManager
		if (!CharacterPreviewManager)
		{
			CharacterPreviewManager = NewObject<UCharacterPreviewManager>(this);
		}
		CharacterPreviewManager->Initialize(this);

		// Remove loading screen after 1.5s
		LoadedWorld->GetTimerManager().SetTimer(
			RemoveLoadingScreenTimerHandle, this,
			&UMyGameInstance::RemoveLoadingScreen, 1.5f, false);

		// Live bot (DEV-ONLY): auto-login + join without typing.
		MaybeStartLiveBot();
	}

	// ---- DevMode / Debug Level ----
	const FName DevModeLevelName = (DevModeConfig.bEnabled && DevModeConfig.LevelOverride != NAME_None)
		? DevModeConfig.LevelOverride
		: DebugLevelName;

	if (LevelBeingLoaded == DevModeLevelName)
	{
		RemoveLoginWidgetFromViewport();
		if (LoadedWorld->GetFirstPlayerController())
		{
			LoadedWorld->GetFirstPlayerController()->bShowMouseCursor = false;
		}

		FClientDataStruct clientData;

		if (DevModeConfig.bEnabled)
		{
			if (!DevModeDataProvider)
			{
				DevModeDataProvider = NewObject<UDevModeDataProvider>(this);
				DevModeDataProvider->Initialize(this, DevModeConfig);
			}

			if (!DevModeDataProvider->LoadPlayerData(clientData))
			{
				UE_LOG(LogGameplay, Warning, TEXT("DevMode: LoadPlayerData failed — using stub data. Check Config/DevMode/dev_player.json"));
				clientData.clientId = 1;
				clientData.characterData.characterId = 1;
				clientData.characterData.characterPosition.positionX = 0.0f;
				clientData.characterData.characterPosition.positionY = 0.0f;
				clientData.characterData.characterPosition.positionZ = 90.0f;
			}
		}
		else
		{
			clientData.clientId = 1;
			clientData.characterData.characterId = 1;
			clientData.characterData.characterPosition.positionX = 0.0f;
			clientData.characterData.characterPosition.positionY = 0.0f;
			clientData.characterData.characterPosition.positionZ = 90.0f;
		}

#if WITH_EDITOR
		if (DevModeConfig.bSpawnAtEditorCameraPosition && GEditor)
		{
			FVector    CamLoc  = FVector::ZeroVector;
			FRotator   CamRot  = FRotator::ZeroRotator;
			bool       bFoundCamera = false;

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
			// DevMode boot skips the online login path that normally seeds the
			// manager world contexts — without this, spawns queue forever.
			// (InitNetworkingSetup never runs here, so gameInstance is set explicitly.)
			if (MOBManager && LoadedWorld) { MOBManager->SetWorldContext(LoadedWorld); MOBManager->SetGameInstance(this); }
			// Phase A bots spawn a stub drop through ItemManager (offline loot
			// has no server itemDrop) — it needs the same seeding.
			if (ItemManager && LoadedWorld) { ItemManager->SetWorldContext(LoadedWorld); ItemManager->SetGameInstance(this); }
			if (DevModeConfig.bSpawnTestMobs && MOBManager)
			{
				DevModeDataProvider->PopulateMobs(MOBManager);
			}
			if (DevModeConfig.bPopulateInventory && InventoryManager)
			{
				DevModeDataProvider->PopulateInventory(InventoryManager, clientData.characterData.characterId);
			}
			if (!DevModeConsoleCommands)
			{
				DevModeConsoleCommands = NewObject<UDevModeConsoleCommands>(this);
				DevModeConsoleCommands->RegisterCommands(this);
			}
			if (!DevModeAutoScenario.IsEmpty())
			{
				// Let the seeded world settle one tick batch, then run the sweep
				// and quit so CI can gate on DEVMODE_SWEEP_RESULT in the log.
				LoadedWorld->GetTimerManager().SetTimer(
					DevModeSweepTimerHandle, this,
					&UMyGameInstance::RunDevModeAutoScenario, 3.0f, false);
				UE_LOG(LogGameplay, Warning, TEXT("GameInstance: auto-scenario '%s' scheduled"), *DevModeAutoScenario);
			}
		}

		// Remove loading screen for DevMode (no ReadyFlags system)
		if (LoadingScreenWidget)
		{
			LoadedWorld->GetTimerManager().SetTimer(
				RemoveLoadingScreenTimerHandle, this,
				&UMyGameInstance::RemoveLoadingScreen, 0.7f, false);
		}
	}

	// WorldMapV1 / game world — MoviePlayer loading screen was started in
	// TransitionToGameWorld() before OpenLevel.  MoviePlayer keeps the widget
	// alive across CleanupWorld — just verify it's still playing and start
	// polling for game-world readiness.
	if (bTransitioningToGameWorld)
	{
		if (!IsLoadingScreenAlive())
		{
			// Headless (-nullrhi) has no renderer: MoviePlayer can never play,
			// so "lost" is expected there — Warning, still reconfigure.
			// A lost screen WITH a renderer is a real travel bug — Error.
			if (!FApp::CanEverRender())
			{
				UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Headless run: loading screen not playing after transition — reconfiguring MoviePlayer"));
			}
			else
			{
				UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] CRITICAL: loading screen lost after level transition — reconfiguring MoviePlayer"));
			}
			SetupMoviePlayerLoadingScreen();
		}
		else
		{
			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Loading screen survived level transition"));
		}

		// The MoviePlayer covers travel and auto-exits at load end (it must never
		// wait for manual stop — see SetupMoviePlayerLoadingScreen). Readiness
		// (gates + flags) is covered by the plain-UMG overlay from here on.
		EnsureReadinessOverlay();

		bGameWorldReady = false;

		// Re-assert foreground AFTER the load: a subsequently launched client
		// steals OS focus mid-load (1s stagger), throttling this window until
		// the user clicks it. (Login level intentionally untouched — the user
		// may be typing in a text field there.)
		ForceGameWindowToFront();

		// Re-arm the keeper for the flags/countdown phase (fresh 8s window).
		StartWindowFocusKeeper();

		// Hard safety watchdog on WALL-CLOCK via the core ticker (never the
		// world TimerManager): an unfocused window starves world timers, so the
		// old world-timer safety could never fire exactly when needed — only an
		// occasional game-thread tick is required. Unconditional: a visible
		// world with pop-in beats an eternal loading screen.
		{
			LoadingScreenWatchdogStartTime = FPlatformTime::Seconds();
			FTSTicker::GetCoreTicker().RemoveTicker(LoadingScreenHardSafetyTickerHandle);
			LoadingScreenHardSafetyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this](float) -> bool
				{
					// Readiness phase uses the plain-UMG overlay; the movie widget
					// may already be gone while the overlay is still up.
					if (!LoadingScreenWidget && !LoadingScreenOverlayWidget) { return false; }
					const double Elapsed = FPlatformTime::Seconds() - LoadingScreenWatchdogStartTime;
					if (!FTravelGate::IsHardWatchdogExpired(Elapsed)) { return true; }
					UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] HARD SAFETY: %.1fs wall-clock elapsed — force-removing loading screen (gates/flags may have stalled)"), Elapsed);
					RemoveLoadingScreen();
					return false;
				}),
				0.5f);
		}

		CheckGameWorldReady();    // first check immediately

		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([this](float) -> bool
			{
				CheckGameWorldReady();
				return !bGameWorldReady;
			}),
			0.033f);
	}
}
