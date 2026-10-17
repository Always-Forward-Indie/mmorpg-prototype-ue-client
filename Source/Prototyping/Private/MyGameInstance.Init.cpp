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


// ============================================================================
// PIE Escape-key preprocessor — intercepts ESC before the editor, preventing
// PIE session from being stopped when the player opens the game menu.
// ============================================================================
class FPIEInputPreprocessor : public IInputProcessor, public TSharedFromThis<FPIEInputPreprocessor>
{
public:
	explicit FPIEInputPreprocessor(UMyGameInstance* InGI) : GI(InGI) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			if (GI && GI->GetWorld())
			{
				APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(GI->GetWorld(), 0);
				if (ABasicPlayer* Player = Cast<ABasicPlayer>(LocalPawn))
				{
					Player->ClearLockedTarget();
					return true;
				}
			}
		}
		return false;
	}

private:
	UMyGameInstance* GI;
};

static TSharedPtr<FPIEInputPreprocessor> GPIEInputPreprocessor;

UMyGameInstance::UMyGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UE_LOG(LogTemp, Warning, TEXT("GameInstance Constructor called"));
}

void UMyGameInstance::Init()
{
	Super::Init();

	// Show the loading screen immediately.  MoviePlayer takes over on the
	// first level transition (LoadLoginLevel) and survives all subsequent
	// CleanupWorld / OpenLevel calls.
	ShowInitialLoadingScreen();
if (FParse::Param(FCommandLine::Get(), TEXT("devmode")))
{
DevModeConfig.bEnabled = true;
UE_LOG(LogTemp, Warning, TEXT("GameInstance: DevMode force-enabled via -devmode command line (skips login, uses Config/DevMode/*.json)"));
}
if (FParse::Value(FCommandLine::Get(), TEXT("devmode="), DevModeAutoScenario) && !DevModeAutoScenario.IsEmpty())
{
DevModeConfig.bEnabled = true;
UE_LOG(LogTemp, Warning, TEXT("GameInstance: DevMode auto-scenario '%s' requested"), *DevModeAutoScenario);
}
if (FParse::Value(FCommandLine::Get(), TEXT("bot-live="), LiveBotScenario) && !LiveBotScenario.IsEmpty())
{
UE_LOG(LogTemp, Warning, TEXT("GameInstance: live-bot scenario '%s' requested (DEV-ONLY: needs -botlogin= -botpass=, dev servers)"), *LiveBotScenario);
}
FString DevModeMapOverride;
if (FParse::Value(FCommandLine::Get(), TEXT("devmode-map="), DevModeMapOverride) && !DevModeMapOverride.IsEmpty())
{
DevModeConfig.bEnabled = true;
DevModeConfig.LevelOverride = FName(*DevModeMapOverride);
UE_LOG(LogTemp, Warning, TEXT("GameInstance: DevMode level override '%s'"), *DevModeMapOverride);
}

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

	// Initialize Idle Timeout system
	IdleTimeoutManager = NewObject<UIdleTimeoutManager>(this);
	if (IdleTimeoutManager)
	{
		IdleTimeoutManager->Configure(this, IdleTimeoutSeconds, IdleWarningSeconds);
	}

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

	// Initialize localization subsystem with the configured data assets
	if (ULocalizationSubsystem* LocSys = GetSubsystem<ULocalizationSubsystem>())
	{
		if (!LocalizationDataAsset && !LocalizationDataAssetRU && !LocalizationDataAssetEN)
		{
			UE_LOG(LogTemp, Error, TEXT("Localization: ALL DataAssets are NULL! Assign at least LocalizationDataAsset in BP_MyGameInstance defaults. Locale switching will not work."));
		}

		LocSys->SetLocalizationData(LocalizationDataAsset);
		LocSys->SetLocalizationDataRU(LocalizationDataAssetRU ? LocalizationDataAssetRU : LocalizationDataAsset);
		LocSys->SetLocalizationDataEN(LocalizationDataAssetEN ? LocalizationDataAssetEN : LocalizationDataAsset);

		// Re-initialize locale now that DataAssets are available.
		// The subsystem's Initialize() runs before this block (and before
		// DataAssets were assigned), so CurrentLocale was never set.
		FString SavedLocale;
		GConfig->GetString(TEXT("Localization"), TEXT("Locale"), SavedLocale, GGameUserSettingsIni);
		LocSys->SetLocale(SavedLocale.IsEmpty() ? TEXT("en") : SavedLocale);
	}

	// Build OS cursor handles from WorldInteractionConfig once, before the login level loads.
	// Handles survive level transitions so they're available immediately in game world too.
	PreloadWorldInteractionCursors();

	UE_LOG(LogTemp, Warning, TEXT("GameInstance Init called"));

	// Register PostLoadMapWithWorld — fires after EVERY OpenLevel completes.
	// Replaces the old OnLoginLevelLoaded streaming callback with a single
	// handler that covers Login, DevMode, and return-to-login.
	PostLoadMapWorldHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UMyGameInstance::OnPostLoginLevelLoaded);

	// Load the initial level after a brief delay so the viewport widget is created
	const float Delay = 0.1f;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(LoadLoginLevelTimerHandle, this, &UMyGameInstance::LoadLoginLevel, Delay, false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GetWorld() returned nullptr, level load delayed."));
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

	// Force time-sync refresh when the game window regains focus.
	// When unfocused, the world FTimerManager is throttled and ping timers
	// starve, causing IsTimeSyncValid() to drop.  Refresh now so cooldowns
	// and skills don't get stuck on the server-epoch/world-time mismatch.
	FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(
		this, &UMyGameInstance::OnApplicationReactivated);
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

		if (!GPIEInputPreprocessor.IsValid())
		{
			GPIEInputPreprocessor = MakeShared<FPIEInputPreprocessor>(this);
			FSlateApplication::Get().RegisterInputPreProcessor(GPIEInputPreprocessor);
			UE_LOG(LogTemp, Log, TEXT("GameInstance: PIE Escape preprocessor registered."));
		}
	}

}

// init networking setup

void UMyGameInstance::Shutdown()
{
	if (GPIEInputPreprocessor.IsValid())
	{
		if (FSlateApplication::IsInitialized())
			FSlateApplication::Get().UnregisterInputPreProcessor(GPIEInputPreprocessor);
		GPIEInputPreprocessor.Reset();
		UE_LOG(LogTemp, Log, TEXT("GameInstance: PIE Escape preprocessor unregistered."));
	}

	if (PreLoadMapDelegateHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapDelegateHandle);
		PreLoadMapDelegateHandle.Reset();
	}

	if (PostLoadMapWorldHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapWorldHandle);
		PostLoadMapWorldHandle.Reset();
	}

	// If this instance held the World Partition transition slot and is shutting
	// down before OnGameWorldReady fires, release it so other PIE instances
	// are not left waiting forever on their retry timer.
	if (bTransitioningToGameWorld)
	{
		FPlatformAtomics::InterlockedExchange(&GActiveWorldPartitionTransitions, 0);
		UE_LOG(LogTemp, Warning, TEXT("Shutdown: released WP transition slot (shutdown during transition)"));
	}

	// Clean up loading-screen frame-countdown delegates so they never fire
	// on the render or game thread after this GameInstance is destroyed.
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
	if (WindowFocusNudgeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WindowFocusNudgeTickerHandle);
		WindowFocusNudgeTickerHandle.Reset();
	}

	// Only send disconnect packets when this PIE instance was actually authenticated.
	// Guards against: (a) PIE stopped before login, (b) crash before auth completes.
	// Each PIE instance owns its own UMyGameInstance so ClientData is per-instance.
	const bool bIsAuthenticated = (ClientData.clientId > 0 && !ClientData.hash.IsEmpty());

	// DevMode offline never connects — no disconnect to send (avoids Error
	// spam from the send path for an expected offline state).
	if (bIsAuthenticated && !DevModeConfig.bEnabled)
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

