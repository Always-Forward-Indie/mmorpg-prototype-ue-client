// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Gameplay/Players/PlayerManager.h"
#include "EngineUtils.h"
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


void UMyGameInstance::CheckGameWorldReady()
{
	UWorld* NewWorld = GetWorld();
	if (!NewWorld) { return; }

	// The MoviePlayer auto-exits at load end by design (see
	// SetupMoviePlayerLoadingScreen) — a finished movie is NOT a problem.
	// What matters is the readiness overlay: re-create it if missing so the
	// player never stares at an unstreamed world.
	EnsureReadinessOverlay();

	// Gate 1: PlayerController must exist before we can do anything.
	APlayerController* PC = NewWorld->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] Gate1 WAIT: no PlayerController yet"));
		return;
	}

	// Spawn the pending local player exactly once as soon as the PC is ready.
	// We do this eagerly so the pawn exists and World Partition can use the
	// player's location as a streaming source to load the surrounding cells.
	if (!bPendingSpawnDispatched && (PendingSpawnClientId > 0 || PendingRemotePlayerSpawns.Num() > 0))
	{
		bPendingSpawnDispatched = true;

		UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 3. Gate1 PASS: PC ready � dispatching spawn (PendingClientId=%d)"), PendingSpawnClientId);

		// World context / managers must be refreshed before spawning so the new
		// world's TimerManager / network polling are active.
		RefreshManagerWorldContexts();
		ProcessPendingSpawns();

		// Phase 3: ACK the server so it sends Phase 4 world-state.
		if (PlayerManager)
		{
			PlayerManager->SendPlayerReadyRequest(ClientData);
			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 4. playerReady sent to server (Phase 3)"));
		}

		// Reset ready-flags and arm the safety fallback timer.
		// If signals (ReadyFlags) never arrive, the loading screen must not hang
		// forever.  After the timeout, remove it IF the world IS ready — the player
		// would rather see the loaded world than a stuck loading screen.
		ReadyFlags = 0;
		AnnouncedGatesMask = 0;
		LoadingScreenShownTime = FPlatformTime::Seconds();
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingScreenSafetyTimerHandle);
		LoadingScreenSafetyTimerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([this](float)
			{
				// Second net: only fires when flags are STILL incomplete.
				// CheckAllReadyFlags hides the screen the moment flags complete
				// and disarms this ticker via RemoveLoadingScreen — but if the
				// unsubscribe was ever missed, never scare the log with a
				// bogus SAFETY: report the real state instead.
				if ((ReadyFlags & ReadyFlag_AllRequired) == ReadyFlag_AllRequired)
				{
					UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] safety tick: flags already complete (0x%02X/0x%02X), standing down"),
						ReadyFlags, ReadyFlag_AllRequired);
					return false;
				}
				// Unconditional by design from here: the player must see the
				// world even if some ReadyFlags never arrived (unfocused window
				// starves the timers and ticks that produce them). The hard 8s
				// watchdog normally fires first; this is the second net.
				const double Elapsed = FPlatformTime::Seconds() - LoadingScreenShownTime;
				UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] SAFETY: %.1fs elapsed without full flags (0x%02X/0x%02X) — force-removing loading screen"),
					Elapsed, ReadyFlags, ReadyFlag_AllRequired);
				RemoveLoadingScreen();
				return false;
			}),
			LoadingScreenSafetyTimeout);
	}

	// Gate 2: Pawn must be possessed � Possess() is called inside SpawnPlayerForClient.
	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] Gate2 WAIT: PC exists but no Pawn yet (Possess not done)"));
		return;
	}

	// Gate PASS logs are once-per-travel (S08): this ticker polls every 33ms,
	// so an unguarded Warning here spams at 30Hz while later gates wait.
	if (!(AnnouncedGatesMask & (1u << 2)))
	{
		AnnouncedGatesMask |= (1u << 2);
		UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Gate2 PASS: Pawn='%s' possessed at Pos=(%.0f,%.0f,%.0f)"),
			*Pawn->GetName(),
			Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z);
	}

	// Gate 3: World Partition streaming must be complete so no geometry pops in
	// during the first rendered frame after the loading screen disappears.
	if (UWorldPartitionSubsystem* WPS = NewWorld->GetSubsystem<UWorldPartitionSubsystem>())
	{
		if (!WPS->IsAllStreamingCompleted())
		{
			UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] Gate3 WAIT: World Partition still streaming..."));
			return;
		}
		if (!(AnnouncedGatesMask & (1u << 3)))
		{
			AnnouncedGatesMask |= (1u << 3);
			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Gate3 PASS: World Partition streaming complete"));
		}
	}
	else
	{
		UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Gate3 SKIP: no WorldPartitionSubsystem (not a WP map)"));
	}

	// Gate 4: Shader compilation must be complete (relevant on first run / cold PSO cache
	// in packaged builds). Skip in Editor where shaders compile in background normally.
#if !WITH_EDITOR
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] Gate4 WAIT: shader compiler still running..."));
		return;
	}
	if (!(AnnouncedGatesMask & (1u << 4)))
	{
		AnnouncedGatesMask |= (1u << 4);
		UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Gate4 PASS: shader compiler idle"));
	}
#endif

	// Gate 5: Texture streaming — prevent mip pop-in on first rendered
	// frames by waiting until all streaming requests are fulfilled.
	{
		const int32 PendingStreaming = IStreamingManager::Get().StreamAllResources(0.0f);
		if (PendingStreaming > 0)
		{
			UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] Gate5 WAIT: %d streaming requests still pending..."), PendingStreaming);
			return;
		}
		if (!(AnnouncedGatesMask & (1u << 5)))
		{
			AnnouncedGatesMask |= (1u << 5);
			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] Gate5 PASS: texture streaming complete"));
		}
	}

	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 5. All gates passed — calling OnGameWorldReady"));
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
	UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] OnGameWorldReady: released WP transition slot"));

	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 6. OnGameWorldReady: bGameWorldReady=true, ReadyFlags=0x%02X � calling CheckAllReadyFlags"), ReadyFlags);

	UWorld* GameWorld = GetWorld();
	if (!GameWorld)
	{
		UE_LOG(LogConnection, Error, TEXT("[LOADSEQ] OnGameWorldReady: GetWorld() is null!"));
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
		UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] OnGameWorldReady: spawning %d deferred remote player(s)"), PendingRemotePlayerSpawns.Num());
		for (const FClientDataStruct& RemoteData : PendingRemotePlayerSpawns)
		{
			AddPlayerData(RemoteData.clientId, RemoteData);
			SpawnPlayerForClient(RemoteData.clientId);
		}
		PendingRemotePlayerSpawns.Empty();
	}

	// Kick the texture streaming system to fully stream textures around the player
	// before the loading screen disappears, preventing mip pop-in on first frame.
	if (UWorld* StreamWorld = GetWorld())
	{
		if (APlayerController* StreamPC = StreamWorld->GetFirstPlayerController())
		{
			if (APawn* StreamPawn = StreamPC->GetPawn())
			{
				IStreamingManager::Get().AddViewInformation(StreamPawn->GetActorLocation(), 1920, 1920);
				IStreamingManager::Get().UpdateResourceStreaming(0.0f, true);
			}
		}
	}

	// All Notify* flags may already be accumulated while streaming was running.
	// Attempt the frame countdown now in case we were the last gate to clear.
	CheckAllReadyFlags();

	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] OnGameWorldReady: complete"));
}

void UMyGameInstance::NotifyPlayerReadyAck()
{
	if (!bPendingSpawnDispatched) { return; }
	ReadyFlags |= ReadyFlag_PlayerReadyAck;
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] FLAG PlayerReadyAck set (mask=0x%02X, bGameWorldReady=%d)"), ReadyFlags, bGameWorldReady);
	CheckAllReadyFlags();
}

void UMyGameInstance::NotifyUIInitialized()
{
	if (!bPendingSpawnDispatched) { return; }
	ReadyFlags |= ReadyFlag_UIInitialized;
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] FLAG UIInitialized set (mask=0x%02X, bGameWorldReady=%d)"), ReadyFlags, bGameWorldReady);
	CheckAllReadyFlags();
}

void UMyGameInstance::NotifyStatsReceived()
{
	if (!bPendingSpawnDispatched) { return; }
	ReadyFlags |= ReadyFlag_StatsReceived;
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] FLAG StatsReceived set (mask=0x%02X, bGameWorldReady=%d)"), ReadyFlags, bGameWorldReady);
	CheckAllReadyFlags();
}

void UMyGameInstance::NotifyPlayerSpawned()
{
	if (!bPendingSpawnDispatched) { return; }
	ReadyFlags |= ReadyFlag_PlayerSpawned;
	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] FLAG PlayerSpawned set (mask=0x%02X, bGameWorldReady=%d)"), ReadyFlags, bGameWorldReady);

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
	UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] CheckAllReadyFlags: mask=0x%02X required=0x%02X bGameWorldReady=%d frameCountdownActive=%d"),
		ReadyFlags, ReadyFlag_AllRequired, (int)bGameWorldReady, EndFrameDelegateHandle.IsValid() ? 1 : 0);

	if (!FReadinessGate::AreAllRequiredSet(ReadyFlags)) { return; }
	// One-shot per screen: after RemoveLoadingScreen both widgets are null, so
	// late Notify* calls (e.g. the 4s stats_update beat) must NOT restart the
	// countdown — without this the screen is "removed" every few seconds and
	// any gate on the removal marker misfires. A fresh transition recreates
	// the widgets, re-arming the path automatically.
	if (!LoadingScreenWidget && !LoadingScreenOverlayWidget) { return; }
	// NOTE: dismissal must never depend on MoviePlayer state. The movie covers
	// travel and auto-exits at load end; waiting on IsMovieCurrentlyPlaying()
	// here (plus re-setup loops) stalled removal whenever the presenter state
	// lagged. StopMovie() in RemoveLoadingScreen handles the movie side.
	EnsureReadinessOverlay();
	if (EndFrameDelegateHandle.IsValid()) { return; }
	if (!bGameWorldReady)
	{
		UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] CheckAllReadyFlags: all flags ready but Gate3 (WP streaming) not done yet"));
		return;
	}

	if (UWorld* W = GetWorld())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LoadingScreenSafetyTimerHandle);
		W->GetTimerManager().ClearTimer(RemoveLoadingScreenTimerHandle);
	}

	UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 7. ALL flags set (0x%02X) + WP streamed � starting frame countdown (%d frames)"),
		ReadyFlags, MinRenderedFramesBeforeHide);
	StartFrameCountdown();
}
void UMyGameInstance::StartFrameCountdown()
{
	RenderedFrameCount.Store(0);
	bLoadingScreenRemovePending.Store(false);
	const double FrameCountdownStartTime = FPlatformTime::Seconds();

	// Count frames on the RENDER THREAD (OnEndFrameRT) so we are certain the
	// loading screen has actually been composited by the GPU before we remove it.
	// OnEndFrameRT fires at the true end of each render frame � not just the end
	// of a game-thread tick � so it reliably reflects what the player has seen.
	EndFrameDelegateHandle = FCoreDelegates::OnEndFrameRT.AddLambda([this]()
	{
		if (bLoadingScreenRemovePending.Load()) { return; }

		const int32 NewCount = RenderedFrameCount.IncrementExchange() + 1;
		UE_LOG(LogConnection, Verbose, TEXT("[LOADSEQ] RT Frame %d / %d"), NewCount, MinRenderedFramesBeforeHide);

		if (NewCount >= MinRenderedFramesBeforeHide)
		{
			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 8. %d RT frames seen � signalling GT to remove loading screen"), MinRenderedFramesBeforeHide);
			bLoadingScreenRemovePending.Store(true);
		}
	});

	// Poll the atomic flag on the game thread once per tick.
	// RemoveLoadingScreen touches UMG and TimerManager � GT only.
	GTRemoveTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this, FrameCountdownStartTime](float) -> bool
		{
			if (!bLoadingScreenRemovePending.Load())
			{
				// Wall-clock safety net: if the render thread is stalled (window
				// unfocused), force-remove after the frame-countdown timeout so
				// the player isn't stuck. Threshold owned by FTravelGate.
				if (!FTravelGate::ShouldGiveUpFrameCountdown(FPlatformTime::Seconds() - FrameCountdownStartTime)) { return true; }
				UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] SAFETY: frame countdown timed out after 3 s � force-removing loading screen"));
				bLoadingScreenRemovePending.Store(true);
			}

			if (EndFrameDelegateHandle.IsValid())
			{
				FCoreDelegates::OnEndFrameRT.Remove(EndFrameDelegateHandle);
				EndFrameDelegateHandle.Reset();
			}

			UE_LOG(LogConnection, Warning, TEXT("[LOADSEQ] 9. GT ticker: removing loading screen NOW"));
			RemoveLoadingScreen();
			return false;   // unregister ticker
		}),
		0.0f);
}
