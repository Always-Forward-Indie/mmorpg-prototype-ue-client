// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/UI/LoginWidget.h"
#include "Gameplay/UI/W_LoginLogoWidget.h"
#include "Gameplay/UI/SocialLinksWidget.h"
#include "Gameplay/UI/LoginFlowWidget.h"
#include "Gameplay/UI/W_LoginScreenOverlayWidget.h"
#include "UI/W_SettingsWidget.h"
#include "Audio/AudioManager.h"
#include "MoviePlayer.h"
#include "Blueprint/UserWidget.h"
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


void UMyGameInstance::PreloadWorldInteractionCursors()
{
	if (!WorldInteractionConfig)
	{
		UE_LOG(LogGameplay, Warning,
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

	UE_LOG(LogGameplay, Verbose,
		TEXT("GameInstance: Cursor preload complete. Default=%s, TypedHandles=%d"),
		PreloadedDefaultCursorHandle ? TEXT("OK") : TEXT("FAILED — check texture settings"),
		PreloadedCursorHandles.Num());
}

// add monitor to viewport
void UMyGameInstance::AddMonitorStatsWidgetToViewport()
{
	if (MonitorStatsWidgetClass)
	{
		if (IsValid(MonitorStatsWidget))
		{
			MonitorStatsWidget->RemoveFromParent();
			MonitorStatsWidget = nullptr;
		}
		MonitorStatsWidget = CreateWidget<UMonitorStatsWidget>(this, MonitorStatsWidgetClass);
		if (MonitorStatsWidget)
		{
			MonitorStatsWidget->AddToViewport();
		}
	}
}


void UMyGameInstance::AddLoginWidgetToViewport()
{
	// Idempotent: PostLoadMap can fire twice for the login map (reloads, PIE
	// re-runs). Without this the widgets double up — the old pointers are
	// overwritten and the orphaned copies stay on screen.
	RemoveLoginWidgetFromViewport();

	// Logo sits at the very bottom (Z-order 5), below the login form.
	if (LoginLogoWidgetClass)
	{
		LoginLogoWidget = CreateWidget<UW_LoginLogoWidget>(this, LoginLogoWidgetClass);
		if (LoginLogoWidget)
		{
			LoginLogoWidget->AddToViewport(5);
		}
	}

	// Social links sit between logo (Z=5) and login form (Z=10).
	if (SocialLinksWidgetClass)
	{
		SocialLinksWidget = CreateWidget<USocialLinksWidget>(this, SocialLinksWidgetClass);
		if (SocialLinksWidget)
		{
			SocialLinksWidget->AddToViewport(6);
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
		// Classic widget path (used when LoginFlowWidgetClass is not set)
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
	if (SocialLinksWidget)
	{
		SocialLinksWidget->RemoveFromParent();
		SocialLinksWidget = nullptr;
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

		UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Loading screen music started (persists across level transition)"));
	}
	else
	{
		UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] StartLoadingScreenMusic: SpawnSound2D returned null"));
	}
}

void UMyGameInstance::ShowInitialLoadingScreen()
{
	if (!LoadingScreenWidgetClass)
	{
		UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] ShowInitialLoadingScreen: SKIP — no widget class"));
		return;
	}

	LoadingScreenWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
	if (!LoadingScreenWidget)
	{
		UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] ShowInitialLoadingScreen: FAIL — CreateWidget returned null"));
		return;
	}

	LoadingScreenWidget->AddToViewport(1000);

	UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] ShowInitialLoadingScreen: widget added to viewport (Init phase, pre-transition)"));
}

void UMyGameInstance::EnsureReadinessOverlay()
{
	if (LoadingScreenOverlayWidget)
	{
		return;
	}
	if (!LoadingScreenWidgetClass)
	{
		UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] EnsureReadinessOverlay: SKIP — no widget class"));
		return;
	}

	LoadingScreenOverlayWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
	if (!LoadingScreenOverlayWidget)
	{
		UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] EnsureReadinessOverlay: FAIL — CreateWidget returned null"));
		return;
	}

	LoadingScreenOverlayWidget->AddToViewport(1001);
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] EnsureReadinessOverlay: readiness overlay shown (movie auto-exited at load end)"));
}

void UMyGameInstance::SetupMoviePlayerLoadingScreen()
{
	if (!LoadingScreenWidgetClass)
	{
		UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] SetupMoviePlayerLoadingScreen: SKIP — no widget class"));
		return;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->RemoveFromParent();
			LoadingScreenWidget = nullptr;
		}

		LoadingScreenWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
		if (!LoadingScreenWidget)
		{
			UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] SetupMoviePlayerLoadingScreen: FAIL — CreateWidget returned null"));
			return;
		}

		LoadingScreenWidget->AddToViewport(1000);
		UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] SetupMoviePlayerLoadingScreen: widget added via AddToViewport (PIE)"));
		StartLoadingScreenMusic();
		return;
	}
#endif

	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
	if (!MoviePlayer)
	{
		UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] SetupMoviePlayerLoadingScreen: FAIL — GetMoviePlayer() returned null"));
		return;
	}

	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
	}

	LoadingScreenWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
	if (!LoadingScreenWidget)
	{
		UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] SetupMoviePlayerLoadingScreen: FAIL — CreateWidget returned null"));
		return;
	}

	TSharedRef<SWidget> SlateWidget = LoadingScreenWidget->TakeWidget();

	FLoadingScreenAttributes LoadingScreen;
	// Auto-exit at load end: with bWaitForManualStop the game thread is trapped
	// inside LoadMap's movie pump (WaitForMovieToFinish spins on
	// bUserCalledFinish, engine does not tick) until the user clicks the
	// screen (OnAnyDown) — eternal "hang until click". Auto-complete lets
	// LoadMap return; readiness is covered by the UMG overlay instead.
	LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
	LoadingScreen.bWaitForManualStop = false;
	// NOTE: bAllowEngineTick must stay FALSE (engine default). TRUE keeps the
	// world rendering behind the movie, which double-ticks the RT geometry
	// manager in one render frame and asserts in RayTracingGeometryManager
	// (all rendered clients died on first frames after load). Poll timers do
	// not need engine tick during load: RestartPolling re-registers them
	// post-load (see RefreshManagerWorldContexts flow).
	LoadingScreen.bAllowEngineTick = false;
	LoadingScreen.WidgetLoadingScreen = SlateWidget;

	MoviePlayer->SetupLoadingScreen(LoadingScreen);

	UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] SetupMoviePlayerLoadingScreen: MoviePlayer configured (survives CleanupWorld)"));

	StartLoadingScreenMusic();
}

bool UMyGameInstance::IsLoadingScreenAlive() const
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return IsValid(LoadingScreenWidget) && LoadingScreenWidget->IsInViewport();
	}
#endif
	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
	return MoviePlayer && MoviePlayer->IsMovieCurrentlyPlaying();
}

void UMyGameInstance::RemoveLoadingScreen()
{
	// Final streaming fence: requests queued AFTER Gate5 passed (pawn, UI
	// widgets, nameplates, camera settle) must not meet the player. Hold the
	// screen a bounded number of extra polls while the queue is non-empty.
	// Login level excluded (no world streaming there); the hard safety
	// timeout stays the last net so this can never hang forever.
	if (LevelBeingLoaded != LoginLevelName && StreamingFenceExtraFrames < StreamingFenceMaxExtraFrames)
	{
		if (IStreamingManager::Get().StreamAllResources(0.0f) > 0)
		{
			++StreamingFenceExtraFrames;
			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] FENCE: streaming still pending at hide time - holding screen (%d/%d)"),
				StreamingFenceExtraFrames, StreamingFenceMaxExtraFrames);
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this](float) -> bool
				{
					RemoveLoadingScreen();
					return false;
				}),
				0.1f);
			return;
		}
	}

	if (LoadingScreenShownTime > 0.0)
	{
		UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] loading screen was up for %.1fs"), FPlatformTime::Seconds() - LoadingScreenShownTime);
		LoadingScreenShownTime = 0.0;
	}
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 9. RemoveLoadingScreen called � loading screen going away NOW"));

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
	if (LoadingScreenHardSafetyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingScreenHardSafetyTickerHandle);
		LoadingScreenHardSafetyTickerHandle.Reset();
	}
	// Soft safety (S11 #125): armed at Gate1, must die here on success or it
	// fires unconditionally after the timeout and double-hides the screen
	// (plus yanks viewport focus mid-game).
	if (LoadingScreenSafetyTimerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingScreenSafetyTimerHandle);
		LoadingScreenSafetyTimerHandle.Reset();
	}
	if (WindowFocusNudgeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WindowFocusNudgeTickerHandle);
		WindowFocusNudgeTickerHandle.Reset();
	}
	bLoadingScreenRemovePending.Store(false);

	// Stop loading music (may already be null if world was torn down by ServerTravel)
	if (IsValid(LoadingScreenAudioComponent))
	{
		LoadingScreenAudioComponent->Stop();
		LoadingScreenAudioComponent->DestroyComponent();
	}
	LoadingScreenAudioComponent = nullptr;

	// Stop MoviePlayer — this hides the loading screen widget.
	if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
	{
		MoviePlayer->StopMovie();
		UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] RemoveLoadingScreen: MoviePlayer stopped"));
	}

	// Restore viewport focus after loading screen removal so chunk server timers
	// and engine ticks resume normally. Fixes window-loses-focus-on-enter bug where
	// chunk server never connects because World TimerManager was starved during load.
	// Skip on login level — the user may be typing in a text field.
	if (FSlateApplication::IsInitialized() && LevelBeingLoaded != LoginLevelName)
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	// Canonical cursor/input-mode sync at the moment the world becomes
	// interactive. OnGameWorldReady already syncs, but UI init can finish
	// later (UIInitialized flag); re-sync here when every window state is
	// final so a login-menu GameAndUI mode can never leak into gameplay.
	// Single source of truth: UIManager decides by open windows.
	if (LevelBeingLoaded != LoginLevelName)
	{
		if (UWorld* GTWorld = GetWorld())
		{
			if (APlayerController* GTPC = GTWorld->GetFirstPlayerController())
			{
				if (APawn* GTPawn = GTPC->GetPawn())
				{
					if (ABasicPlayer* GTPlayer = Cast<ABasicPlayer>(GTPawn))
					{
						if (UUIManager* GTUIMgr = GTPlayer->GetUIManager())
						{
							GTUIMgr->UpdateCursorAndInputMode();
						}
					}
				}
			}
		}
	}

	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
	}

	if (LoadingScreenOverlayWidget)
	{
		LoadingScreenOverlayWidget->RemoveFromParent();
		LoadingScreenOverlayWidget = nullptr;
	}
}
