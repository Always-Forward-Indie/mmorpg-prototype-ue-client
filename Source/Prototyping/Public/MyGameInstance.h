#pragma once

#include "CoreMinimal.h"

#include "Networking/NetworkManager.h"
#include "Authentication/AuthenticationManager.h"
#include "Gameplay/Players/PlayerManager.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Mobs/SpawnZoneManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "UI/UIManager.h"
#include "Services/TimeSyncService.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioConfigDataAsset.h"
#include "DevMode/DevModeDataProvider.h"

#include <Kismet/GameplayStatics.h>
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h" 
#include "Widgets/SWeakWidget.h"

#include "Gameplay/Players/MyCameraActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/UI/LoginWidget.h"
#include "Gameplay/UI/LoginFlowWidget.h"
#include "Gameplay/UI/W_LoginScreenOverlayWidget.h"
#include "Gameplay/UI/W_LoginLogoWidget.h"
#include "UI/W_SettingsWidget.h"
#include "Gameplay/UI/CharacterListItem.h"
#include "Gameplay/UI/MonitorStatsWidget.h"
#include "Components/ListView.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.generated.h"


// Delegate to handle the login response
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginResponseReceived, int32, ClientID, const FString&, ResponseMessage);

// Delegate to handle the game server response
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameServerResponseReceived, int32, ClientID, const FString&, ResponseMessage);

/**
 *
 */

class UFloatingCombatTextManager;
class ADroppedItemActor;
class UCharacterPreviewManager;

UCLASS()
class PROTOTYPING_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

private:
FTimerHandle LoadLoginLevelTimerHandle; // Timer handle for loading the login level

FTimerHandle RemoveLoadingScreenTimerHandle; // Timer handle for loading the game level

FTimerHandle NetworkServersPingTimerHandle; // Timer handle for ping game server

FTimerHandle GameWorldReadyTimerHandle; // Timer handle for polling game world readiness

// ClientData
FClientDataStruct ClientData;

// Client ID
int32 CurrentClientID;

// Client token
FString CurrentClientSecret;

// Client login
FString CurrentClientLogin;

// Client Character ID
int32 CurrentCharacterID;

// Time variables to measure ping
FDateTime SendTimeGameServer;
FDateTime SendTimeLoginServer;
FDateTime ReceiveTimeGameServer;
FDateTime ReceiveTimeLoginServer;

	// --- Join flow state ---
	// True once JoinSelectedCharacterToGame has fired its joinGameClient request.
	// Prevents a second click (or accidental double-call) from sending duplicate
	// joinGameCharacter packets before the server flow completes.
	bool bJoinGameInProgress = false;

	// --- Level transition state ---
	// True while we are in the process of transitioning from Login to GameWorld
	bool bTransitioningToGameWorld = false;

	// True once the game world (WorldMapV1) is fully loaded and ready for gameplay
	bool bGameWorldReady = false;

	// True once CheckGameWorldReady has dispatched the pending player spawn.
	// Prevents SpawnPlayerForClient / RefreshManagerWorldContexts being called
	// more than once per session from the 200ms polling ticker.
	bool bPendingSpawnDispatched = false;

	// Bitmask tracking which subsystems have finished initializing.
	// Loading screen is hidden only when ALL required flags are set.
	// 
	//  Bit 0 (0x01) � playerReady ACK received (Phase 3 > Phase 4 started)
	//  Bit 1 (0x02) � local player UI fully initialized (UIInitTimerHandle fired)
	//  Bit 2 (0x04) � first stats_update received and HUD refreshed
	//  Bit 3 (0x08) � local player actor physically spawned and present in world
	//
	static constexpr uint8 ReadyFlag_PlayerReadyAck = 0x01;
	static constexpr uint8 ReadyFlag_UIInitialized  = 0x02;
	static constexpr uint8 ReadyFlag_StatsReceived  = 0x04;
	static constexpr uint8 ReadyFlag_PlayerSpawned  = 0x08;
	static constexpr uint8 ReadyFlag_AllRequired    = ReadyFlag_PlayerReadyAck
	                                                | ReadyFlag_UIInitialized
	                                                | ReadyFlag_PlayerSpawned;  // StatsReceived excluded
	uint8 ReadyFlags = 0;

	// Safety fallback timer: removes loading screen even if some signals never arrive
	FTimerHandle LoadingScreenSafetyTimerHandle;

	// Number of render-thread frames observed since all ReadyFlags were set.
	// Counted on the render thread via OnEndFrameRT; removal dispatched back to
	// the game thread so UMG / TimerManager are touched only from GT.
	TAtomic<int32> RenderedFrameCount{ 0 };
	static constexpr int32 MinRenderedFramesBeforeHide = 3;

	// Set to true by the render-thread callback once MinRenderedFramesBeforeHide
	// frames have been seen. The next game-thread Tick reads this and removes
	// the loading screen safely.
	TAtomic<bool> bLoadingScreenRemovePending{ false };

	// Handle for the OnEndFrameRT delegate that counts real render frames.
	FDelegateHandle EndFrameDelegateHandle;

	// Game-thread ticker that polls bLoadingScreenRemovePending once per tick
	// and calls RemoveLoadingScreen when the render thread has seen enough frames.
	FTSTicker::FDelegateHandle GTRemoveTicker;

	// Handle for the FCoreUObjectDelegates::PreLoadMap callback that nulls out
	// world-dependent pointers before the old world is torn down.
	FDelegateHandle PreLoadMapDelegateHandle;

	// Timer used to retry OpenLevel if another PIE instance is already in the
	// middle of World Partition GenerateStreaming (prevents WorldDataLayers ensure).
	FTimerHandle OpenLevelRetryTimerHandle;

	// Performs the actual UGameplayStatics::OpenLevel call and arms the ready-ticker.
	// Called directly by TransitionToGameWorld, or deferred by the retry timer.
	void DoOpenLevel();

	// Begin counting rendered frames; removes loading screen after MinRenderedFramesBeforeHide.
	void StartFrameCountdown();

public:
	// Client ID of the local player that needs to be spawned once game world is ready
	int32 PendingSpawnClientId = 0;

	// Pending spawn data for players that arrived during level transition
	TArray<FClientDataStruct> PendingRemotePlayerSpawns;

public:
	UMyGameInstance(const FObjectInitializer& ObjectInitializer);

	void Init();

	/**
	 * Called by the engine after the first map has finished loading (login level).
	 * At this point the game viewport widget is fully in the Slate tree, so we can
	 * safely pass focus to it and make the custom cursor visible without waiting
	 * for a BasicPlayer / CursorInteractionComponent to exist.
	 */
	void OnStart() override;

	void InitNetworkingSetup();

	void InitGameSystems();

	void Shutdown();

	// Refresh WorldContext on all managers after a level transition
	void RefreshManagerWorldContexts();

	// Null out worldContext on all managers BEFORE the old world is destroyed.
	// Called from the FCoreUObjectDelegates::PreLoadMap callback so in-flight
	// network packets cannot dereference a stale TObjectPtr<UWorld> handle.
	void InvalidateManagerWorldContexts(const FString& MapName);

	// Called when the game world is fully loaded and ready for gameplay
	void OnGameWorldReady();

	// Polls whether the game world is ready after OpenLevel
	void CheckGameWorldReady();

	// Process any player spawns that were queued during level transition
	void ProcessPendingSpawns();

	// Returns true if the game world is loaded and ready
	bool IsGameWorldReady() const { return bGameWorldReady; }

	// Called by PlayerManager when the server sends playerReady ACK (Phase 3 complete).
	void NotifyPlayerReadyAck();

	// Called by BasicPlayer after UIInitTimerHandle fires and all widgets are initialized.
	void NotifyUIInitialized();

	// Called by BasicPlayer after the first stats_update is processed and HUD is refreshed.
	void NotifyStatsReceived();

	// Called by BasicPlayer once the local pawn is physically present in the game world
	// (UIInitTimer has fired, all widgets are up). Triggers the frame-counter gate.
	void NotifyPlayerSpawned();

private:
	// Checks whether all ReadyFlags are set and hides the loading screen if so.
	void CheckAllReadyFlags();

public:

	// Duration (seconds) to keep the loading screen up as a last-resort safety net.
	// Configurable in Blueprint defaults � no rebuild needed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (ClampMin = "5.0", ClampMax = "30.0"))
	float LoadingScreenSafetyTimeout = 15.0f;

	UPROPERTY()
	// Network manager
	UNetworkManager* NetworkManager;

	UPROPERTY()
	// Ping manager
	UPingManager* PingManager;

	UPROPERTY()
	// Authentication manager
	UAuthenticationManager* AuthenticationManager;

	UPROPERTY()
	// Player manager
	UPlayerManager* PlayerManager;

	UPROPERTY()
	// MOB manager
	UMOBManager* MOBManager;

	// Zone manager
	UPROPERTY()
	USpawnZoneManager* SpawnZoneManager;

	// Item manager
	UPROPERTY()
	UItemManager* ItemManager;

	// Inventory manager
	UPROPERTY()
	UInventoryManager* InventoryManager;

	// Harvest manager
	UPROPERTY()
	class UHarvestManager* HarvestManager;

	// Experience manager
	UPROPERTY()
	class UExperienceManager* ExperienceManager;

	// Experience network handler
	UPROPERTY()
	class UExperienceNetworkHandler* ExperienceNetworkHandler;

	// Combat system managers
	UPROPERTY()
	class UCombatSystemManager* CombatSystemManager;

	UPROPERTY()
	class USkillSystemManager* SkillSystemManager;

	UPROPERTY()
	class UCombatNetworkHandler* CombatNetworkHandler;

	// Player skill system components
	UPROPERTY()
	class UPlayerSkillManager* PlayerSkillManager;

	UPROPERTY()
	class USkillDefinitionRepository* SkillDefinitionRepository;

	UPROPERTY()
	class UEntityAudioRepository* EntityAudioRepositoryRef;

	UPROPERTY()
	class UPlayerSkillNetworkHandler* PlayerSkillNetworkHandler;

	UPROPERTY()
	class UPlayerSkillSystemFactory* PlayerSkillSystemFactory;

	// Time synchronization service
	UPROPERTY()
	UTimeSyncService* TimeSyncService;

	// NPC System
	UPROPERTY()
	class UNPCManager* NPCManager;

	UPROPERTY()
	class UNPCNetworkHandler* NPCNetworkHandler;

	// Audio system
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	UAudioManager* AudioManager;

	/** Data Asset with all designer-facing audio settings.
	 *  Create DA_AudioConfig in Content Browser and assign it here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	UAudioConfigDataAsset* AudioConfig;

	// ─────────────────────────────────────────────────────────────────────────
	// World Interaction / Cursor system
	// ─────────────────────────────────────────────────────────────────────────

	/**
	 * Cursor icons, decal settings and interaction ranges.
	 * Assign DA_WorldInteractionConfig HERE (in BP_GameInstance) instead of on each
	 * player Blueprint — the asset is then loaded once for the whole session and
	 * cursor handles survive level transitions (login → game world).
	 *
	 * CursorInteractionComponent on the player will also accept a local Config override, 
	 * but if left empty it will fall back to this GameInstance config automatically.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Interaction")
	class UWorldInteractionConfig* WorldInteractionConfig;

	/**
	 * Preloaded OS-level cursor handles built from WorldInteractionConfig.
	 * On Windows each entry is an HCURSOR.  Built once in Init() so they are
	 * available in the login level and survive level transitions.
	 * void* keeps platform headers out of this header.
	 */
	TMap<uint8, void*> PreloadedCursorHandles;

	/** Default cursor handle (nothing under cursor). */
	void* PreloadedDefaultCursorHandle = nullptr;

	/** Build handles from WorldInteractionConfig. Called once at the end of Init(). */
	void PreloadWorldInteractionCursors();

	// Player stats manager
	UPROPERTY()
	class UPlayerStatsManager* PlayerStatsManager;

	// Player stats network handler
	UPROPERTY()
	class UPlayerStatsNetworkHandler* PlayerStatsNetworkHandler;

	// Mastery manager
	UPROPERTY()
	class UMasteryManager* MasteryManager;

	// Mastery network handler
	UPROPERTY()
	class UMasteryNetworkHandler* MasteryNetworkHandler;

	// Reputation manager
	UPROPERTY()
	class UReputationManager* ReputationManager;

	// Reputation network handler
	UPROPERTY()
	class UReputationNetworkHandler* ReputationNetworkHandler;

	// Titles manager
	UPROPERTY()
	class UTitleManager* TitleManager;

	// Titles network handler
	UPROPERTY()
	class UTitleNetworkHandler* TitleNetworkHandler;

	// Dialogue manager
	UPROPERTY()
	class UDialogueManager* DialogueManager;

	// Dialogue network handler
	UPROPERTY()
	class UDialogueNetworkHandler* DialogueNetworkHandler;

	// Quest manager
	UPROPERTY()
	class UQuestManager* QuestManager;

	// Quest network handler
	UPROPERTY()
	class UQuestNetworkHandler* QuestNetworkHandler;

	// Equipment manager
	UPROPERTY()
	class UEquipmentManager* EquipmentManager;

	// Equipment network handler
	UPROPERTY()
	class UEquipmentNetworkHandler* EquipmentNetworkHandler;

	// Vendor manager
	UPROPERTY()
	class UVendorManager* VendorManager;

	// Vendor network handler
	UPROPERTY()
	class UVendorNetworkHandler* VendorNetworkHandler;

	// Repair manager
	UPROPERTY()
	class URepairManager* RepairManager;

	// Repair network handler
	UPROPERTY()
	class URepairNetworkHandler* RepairNetworkHandler;

	// Skill shop manager (NPC trainer)
	UPROPERTY()
	class USkillShopManager* SkillShopManager;

	// Skill shop network handler
	UPROPERTY()
	class USkillShopNetworkHandler* SkillShopNetworkHandler;

	// Trade manager
	UPROPERTY()
	class UTradeManager* TradeManager;

	// Trade network handler
	UPROPERTY()
	class UTradeNetworkHandler* TradeNetworkHandler;

	// Bestiary network handler
	UPROPERTY()
	class UBestiaryNetworkHandler* BestiaryNetworkHandler;

	// Chat manager
	UPROPERTY()
	class UChatManager* ChatManager;

	// Chat network handler
	UPROPERTY()
	class UChatNetworkHandler* ChatNetworkHandler;

	// Emote manager
	UPROPERTY()
	class UEmoteManager* EmoteManager;

	// Emote network handler
	UPROPERTY()
	class UEmoteNetworkHandler* EmoteNetworkHandler;

	// NPC Ambient Speech manager
	UPROPERTY()
	class UAmbientSpeechManager* AmbientSpeechManager;

	// NPC Ambient Speech network handler
	UPROPERTY()
	class UAmbientSpeechNetworkHandler* AmbientSpeechNetworkHandler;

	// World Interactive Objects manager
	UPROPERTY()
	class UWorldObjectManager* WorldObjectManager;

	// World Interactive Objects network handler
	UPROPERTY()
	class UWIONetworkHandler* WIONetworkHandler;

	// DevMode configuration (editable in Blueprint defaults)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevMode")
	FDevModeConfig DevModeConfig;

	// DevMode runtime objects (created on demand when DevMode is enabled)
	UPROPERTY()
	UDevModeDataProvider* DevModeDataProvider = nullptr;

	UPROPERTY()
	class UDevModeConsoleCommands* DevModeConsoleCommands = nullptr;

	TMap<int32, FClientDataStruct> ConnectedPlayers;

	UPROPERTY()
	TMap<int32, ABasicPlayer*> SpawnedPlayers;

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnLoginResponseReceived OnLoginResponseReceived;

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnGameServerResponseReceived OnGameServerResponseReceived;

	// get network manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UNetworkManager* GetNetworkManager();

	// get Authentication manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UAuthenticationManager* GetAuthenticationManager();

	// get Player manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UPlayerManager* GetPlayerManager();

	// get MOB manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UMOBManager* GetMOBManager();

	// get baic mob class
	UFUNCTION(BlueprintCallable, Category = "MOB")
	TSubclassOf<class ABasicMOB> GetBasicMOBClass();

	// get NPC class
	UFUNCTION(BlueprintCallable, Category = "NPC")
	TSubclassOf<class ABasicNPC> GetBasicNPCClass();

	// get spawn zone manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	USpawnZoneManager* GetSpawnZoneManager();

	// get basic SpawnZone class
	UFUNCTION(BlueprintCallable, Category = "MOB")
	TSubclassOf<class AMobSpawnZone> GetBasicSpawnZoneClass();

	// get dropped item actor class
	UFUNCTION(BlueprintCallable, Category = "Items")
	TSubclassOf<class ADroppedItemActor> GetDroppedItemActorClass();

	// get item manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UItemManager* GetItemManager();

	// get inventory manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	UInventoryManager* GetInventoryManager();

	// get harvest manager
	UFUNCTION(BlueprintCallable, Category = "Network")
	class UHarvestManager* GetHarvestManager();

	// get experience manager
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UExperienceManager* GetExperienceManager();

	// get experience network handler
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UExperienceNetworkHandler* GetExperienceNetworkHandler();

	// get mastery manager
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UMasteryManager* GetMasteryManager() { return MasteryManager; }

	// get mastery network handler
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UMasteryNetworkHandler* GetMasteryNetworkHandler() { return MasteryNetworkHandler; }

	// get reputation manager
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UReputationManager* GetReputationManager() { return ReputationManager; }

	// get reputation network handler
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UReputationNetworkHandler* GetReputationNetworkHandler() { return ReputationNetworkHandler; }

	// get title manager
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UTitleManager* GetTitleManager() { return TitleManager; }

	// get title network handler
	UFUNCTION(BlueprintCallable, Category = "Player Progression")
	class UTitleNetworkHandler* GetTitleNetworkHandler() { return TitleNetworkHandler; }

	// get emote manager
	UFUNCTION(BlueprintCallable, Category = "Emotes")
	class UEmoteManager* GetEmoteManager() { return EmoteManager; }

	// get emote network handler
	UFUNCTION(BlueprintCallable, Category = "Emotes")
	class UEmoteNetworkHandler* GetEmoteNetworkHandler() { return EmoteNetworkHandler; }

	/**
	 * Emote action routing: called by EmoteManager::OnEmoteActionReceived.
	 * Finds the ABasicPlayer with matching characterId in SpawnedPlayers and triggers PlayEmoteForCharacter.
	 */
	UFUNCTION()
	void RouteEmoteActionToPlayer(int32 CharacterId, const FString& EmoteSlug, const FString& AnimationName);

	// get combat system manager
	UFUNCTION(BlueprintCallable, Category = "Combat")
	class UCombatSystemManager* GetCombatSystemManager();

	// get skill system manager
	UFUNCTION(BlueprintCallable, Category = "Combat")
	class USkillSystemManager* GetSkillSystemManager();

	// get combat network handler
	UFUNCTION(BlueprintCallable, Category = "Combat")
	class UCombatNetworkHandler* GetCombatNetworkHandler();

	// get player skill manager
	UFUNCTION(BlueprintCallable, Category = "Player Skills")
	class UPlayerSkillManager* GetPlayerSkillManager();

	// get skill definition repository
	UFUNCTION(BlueprintCallable, Category = "Player Skills")
	class USkillDefinitionRepository* GetSkillDefinitionRepository();

	// get player skill network handler
	UFUNCTION(BlueprintCallable, Category = "Player Skills")
	class UPlayerSkillNetworkHandler* GetPlayerSkillNetworkHandler();

	// get player skill system factory
	UFUNCTION(BlueprintCallable, Category = "Player Skills")
	class UPlayerSkillSystemFactory* GetPlayerSkillSystemFactory();

	// get current client data
	UFUNCTION(BlueprintCallable, Category = "Client Data")
	FClientDataStruct GetCurrentClientData();

	void SetCurrentClientID(int32 ClientID);

	int32 GetCurrentClientID();

	void SetCurrentClientHash(FString ClientSecret);

	FString GetCurrentClientHash();

	void AddPlayerData(int32 ClientID, const FClientDataStruct clientData);

	void RemovePlayerData(int32 ClientID);

	void MovePlayerForClient(const int32 ClientID, const FClientDataStruct& clientData, const FMessageDataStruct& MessageData);

	void HandlePlayerDisconnection(int32 ClientID);

	void UpdatePlayerCoordinates(int32 PlayerID, double x, double y, double z, double rotZ);

	void SetCharacterItems(TArray<FCharacterDataStruct> Items);

	// spawn players
	void SpawnPlayerForClient(int32 ClientID);

	// Load a streaming sub-level (Login, Debug). NOT used for World Partition game maps.
	UFUNCTION(BlueprintCallable, Category = "Level")
	void LoadStreamingLevel(const FName& LevelName);

	// Transition to the game world map (World Partition) via OpenLevel
	UFUNCTION(BlueprintCallable, Category = "Level")
	void TransitionToGameWorld();

	void LoadLoginLevel();

	UFUNCTION(BlueprintCallable, Category = "Level")
	void OnLoginLevelLoaded();

	UFUNCTION(BlueprintCallable, Category = "Level")
	void OnLevelUnloaded();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void AddMonitorStatsWidgetToViewport();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void AddLoginWidgetToViewport();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RemoveLoginWidgetFromViewport();

	/** Show the login settings window (creates it on first call). */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowLoginSettings(ESettingsTab Tab = ESettingsTab::Audio);

	/** Hide the login settings window. */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideLoginSettings();

	void AddLoadingScreen();

	void RemoveLoadingScreen();

	// Starts loading screen background music that persists across ServerTravel.
	void StartLoadingScreenMusic();

	UFUNCTION(BlueprintCallable, Category = "Character")
	void SetCurrentCharacterID(int32 CharacterID);

	UFUNCTION(BlueprintCallable, Category = "Character")
	int32 GetCurrentCharacterID();

	// Join the selected character to the game
	UFUNCTION(BlueprintCallable, Category = "Game")
	void JoinSelectedCharacterToGame();

	// Game world map asset (World Partition). Pick the map directly in the Details panel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> GameWorldMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FName LoginLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FName DebugLevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

	UPROPERTY()
	UUserWidget* LoadingScreenWidget;

	/** Legacy login widget class (kept for backward compat, prefer LoginFlowWidgetClass). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<ULoginWidget> LoginScreenWidgetClass;

	UPROPERTY()
	ULoginWidget* LoginScreenWidget;

	/** New login flow widget class with WidgetSwitcher (Login/Register/CharSelect/CharCreate).
	 *  If set, this takes priority over LoginScreenWidgetClass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "Login Flow Widget Class"))
	TSubclassOf<ULoginFlowWidget> LoginFlowWidgetClass;

	UPROPERTY()
	ULoginFlowWidget* LoginFlowWidget;

	/** DataTable with row struct FLoginErrorTableRow.
	 *  Row names = server error codes (ERR_LOGIN_TAKEN, ERR_CHAR_NAME_INVALID, etc.).
	 *  Automatically passed to LoginFlowWidget on creation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "Login Error Messages Table"))
	UDataTable* LoginErrorMessagesTable;

	/** Overlay widget shown on top of the login/character-select screen.
	 *  Assign WBP_LoginScreenOverlay (subclass of UW_LoginScreenOverlayWidget).
	 *  Contains Settings and Exit Game buttons. Leave empty to disable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "Login Screen Overlay Widget Class"))
	TSubclassOf<UW_LoginScreenOverlayWidget> LoginScreenOverlayWidgetClass;

	UPROPERTY()
	UW_LoginScreenOverlayWidget* LoginScreenOverlayWidget;

	/** Game logo widget shown on the login screen (Z-order 5, behind the login form).
	 *  Assign WBP_LoginLogo (subclass of UW_LoginLogoWidget). Leave empty to disable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "Login Logo Widget Class"))
	TSubclassOf<UW_LoginLogoWidget> LoginLogoWidgetClass;

	UPROPERTY()
	UW_LoginLogoWidget* LoginLogoWidget;

	/** Tabbed settings window shown when the player clicks the Settings button
	 *  on the login screen overlay.
	 *  Assign WBP_Settings (subclass of UW_SettingsWidget). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (DisplayName = "Login Settings Widget Class"))
	TSubclassOf<UW_SettingsWidget> LoginSettingsWidgetClass;

	UPROPERTY()
	UW_SettingsWidget* LoginSettingsWidget;

	// ─── Character Visual System ─────────────────────────────────────────────

	/** DataTable with row struct FCharacterVisualDefinition.
	 *  Row names = "classSlug_raceSlug_genderName" (e.g. "warrior_human_male").
	 *  Used by login preview AND in-game player spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Visuals", meta = (DisplayName = "Character Visual Definitions"))
	UDataTable* CharacterVisualDefinitionsTable;

	/** DataTable with row struct FCharacterCosmeticData.
	 *  Row names = cosmetic slug (e.g. "hair_human_female_01", "beard_human_male_01").
	 *  Drives UCosmeticVisualComponent — hair, facial hair, etc. that hide under helmets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Visuals", meta = (DisplayName = "Character Cosmetics"))
	UDataTable* CharacterCosmeticsDataTable;

	/** Character preview manager for the login screen podium. Created on login level load. */
	UPROPERTY()
	UCharacterPreviewManager* CharacterPreviewManager;

	/** Podium spawn locations for character select preview (up to 4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Podium")
	TArray<FVector> PodiumSpawnLocations;

	/** Common facing rotation for podium characters (fallback / legacy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Podium")
	FRotator PodiumSpawnRotation = FRotator(0.0f, 180.0f, 0.0f);

	/** Per-slot facing rotations populated from LoginLevelSetupActor arrows.
	 *  Index matches PodiumSpawnLocations.  Falls back to PodiumSpawnRotation
	 *  when not populated. */
	UPROPERTY(BlueprintReadOnly, Category = "Character Visuals|Podium")
	TArray<FRotator> PodiumSpawnRotations;

	/** Camera position when viewing the podium (character select). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Podium")
	FVector PodiumCameraLocation;

	/** Camera rotation when viewing the podium (character select). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Podium")
	FRotator PodiumCameraRotation;

	/** Where the single character-create preview spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Create Preview")
	FVector CreatePreviewLocation;

	/** Facing rotation for the character-create preview. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Create Preview")
	FRotator CreatePreviewRotation = FRotator(0.0f, 180.0f, 0.0f);

	/** Camera position for close-up character-create view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Create Preview")
	FVector CreatePreviewCameraLocation;

	/** Camera rotation for close-up character-create view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Create Preview")
	FRotator CreatePreviewCameraRotation;

	/**
	 * The spot a selected character walks/teleports to when chosen in character select.
	 * Placed between the podium and the camera so the character appears to step forward.
	 * Read from ALoginLevelSetupActor::SelectedCharacterSlot (Red arrow).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Podium")
	FVector SelectedCharacterLocation;

	/** Facing rotation for the selected character (e.g. angled slightly toward camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Visuals|Podium")
	FRotator SelectedCharacterRotation = FRotator(0.0f, 180.0f, 0.0f);

	/**
	 * Widget class used as the nameplate canvas in the login level.
	 * Assign your WBP_NameplateCanvas (or any UNameplateCanvasWidget subclass) here.
	 * When set, CharacterPreviewManager will spawn it and register each preview actor.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Visuals|Podium")
	TSubclassOf<UUserWidget> LoginNameplateCanvasClass;

	// ─────────────────────────────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UCharacterListItem> CharactersListItemWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UMonitorStatsWidget> MonitorStatsWidgetClass;

	UPROPERTY()
	UMonitorStatsWidget* MonitorStatsWidget;

	// ����� �������, ������� �������� � Blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UMessageBoxPopup> MessageBoxPopupClass;


	// A member variable to store the name of the level being loaded
	FName LevelBeingLoaded;

	// Camera actor for the login level
	UPROPERTY()
	AMyCameraActor* LoginLevelCamera;

	// Player actor for the game level
	UPROPERTY()
	ABasicPlayer* Player;

	// add a reference to the player actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	TSubclassOf<class ABasicPlayer> MainPlayerClass;

	// add a reference to the MOB actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	TSubclassOf<class ABasicNPC> BasicNPCClass;

	// add a reference to the MOB actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MOB")
	TSubclassOf<class ABasicMOB> BasicMOBClass;

	// add a reference to the spawn zone actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MOB")
	TSubclassOf<class AMobSpawnZone> BasicSpawnZoneClass;

	// add a reference to the dropped item actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	TSubclassOf<class ADroppedItemActor> DroppedItemActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	UDataTable* ItemVisualsDataTable;

	// Skill definitions data table
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Skills")
	UDataTable* SkillDefinitionsDataTable;

	// Footstep sounds data table (row struct = FFootstepSoundData)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	UDataTable* FootstepSoundsTable;

	// Impact sounds data table (row struct = FImpactSoundData)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	UDataTable* ImpactSoundsTable;

	/** Entity audio profiles data table (row struct = FEntityAudioProfile).
	 *  Create a DataTable with this row struct and assign it here in the GameInstance Blueprint.
	 *  Row keys: "warrior_m", "wolf", "goblin_shaman", etc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	UDataTable* EntityAudioProfilesTable;

	/** Per-entity per-skill voice override table (row struct = FEntitySkillVoiceOverride).
	 *  Optional. Row key format: "{audioProfileId}|{skillSlug}", e.g. "warrior_m|fireball".
	 *  When a row is found it is used as Priority 2 between skill-global voice (P1)
	 *  and the entity\'s generic voice pool (P3).
	 *  Leave unassigned to skip per-skill overrides entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	UDataTable* EntitySkillVoiceOverridesTable;

	// Effect definition data table (row struct = FEffectDefinitionRow)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay")
	UDataTable* EffectDefinitionTable;

	/** Returns the FootstepSoundsTable for use by anim notifies. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio")
	UDataTable* GetFootstepSoundsTable() const { return FootstepSoundsTable; }

	/** Returns the ImpactSoundsTable for weapon impact sound lookup. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio")
	UDataTable* GetImpactSoundsTable() const { return ImpactSoundsTable; }

	/** Returns the EntityAudioRepository for profile lookup by FName. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	class UEntityAudioRepository* GetEntityAudioRepository();

	/** Returns the EffectDefinitionTable for buff/debuff VFX and sound lookup. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Gameplay")
	UDataTable* GetEffectDefinitionTable() const { return EffectDefinitionTable; }

	/** Returns the ItemVisualsDataTable for item visual data lookup. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Items")
	UDataTable* GetItemVisualsDataTable() const { return ItemVisualsDataTable; }

	// add a reference to the camera actor blueprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	TSubclassOf<class AMyCameraActor> LoginCameraClass;

	// camera rotation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	FRotator LoginLevelCameraRotation; // -15.0f, 235.0f, 0.0f

	// camera location
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	FVector LoginLevelCameraLocation; //1675.0f, -190.0f, 605.0f

	// player location
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	FVector GamePlayerLocation;

	// player rotation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	FRotator GamePlayerRotation; // 0.0f, 0.0f, 0.0f

	// sound cue for the login screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* LoginMusicSoundSource;

	// sound cue for the loading screen
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	USoundBase* LoadingMusicSoundSource;

	// debug flag
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDebug;

	// Slate handle used to keep the loading-screen widget in the viewport
	// overlay via UGameViewportClient::AddViewportWidgetContent().
	// This survives level transitions (OpenLevel) since it is world-independent.
	TSharedPtr<SWidget> LoadingScreenSlateWidget;

	// Audio component for loading screen music.
	// Owned by the GameInstance so it survives world travel (unlike actors).
	UPROPERTY()
	UAudioComponent* LoadingScreenAudioComponent = nullptr;

	// Ping servers
	UFUNCTION()
	void StartPingGameServer();
	UFUNCTION()
	void StartPingLoginServer();

	public:
		// Combat system functions (updated for new system)
		UFUNCTION(BlueprintCallable, Category = "Combat")
		void PlayCombatAnimation(const FCombatAnimationData& AnimationData);

		UFUNCTION(BlueprintCallable, Category = "Combat")
		void ProcessCombatAction(const FCombatActionData& ActionData);

		// Health update methods (supporting both new and legacy systems)
		UFUNCTION(BlueprintCallable, Category = "Combat")
		void UpdateMobHealth(int32 TargetId, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt);

		UFUNCTION(BlueprintCallable, Category = "Combat")
		void UpdatePlayerHealth(int32 TargetId, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt);

		UFUNCTION(BlueprintCallable, Category = "Combat")
		void UpdateTargetHealth(int32 TargetId, int32 TargetType, const FString& TargetTypeString, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt);

		// Set inventory manager reference
		UFUNCTION(BlueprintCallable, Category = "Network")
		void SetInventoryManager(UInventoryManager* NewInventoryManager);

		// get time sync service
		UFUNCTION(BlueprintCallable, Category = "Network")
		UTimeSyncService* GetTimeSyncService();

		// Process time sync data from server responses
		UFUNCTION(BlueprintCallable, Category = "Network")
		void ProcessTimeSyncData(const FMessageDataStruct& MessageData);

		// Get player by character ID
		UFUNCTION(BlueprintCallable, Category = "Player")
		ABasicPlayer* GetPlayerByCharacterId(int32 CharacterId);

		// NPC System getters
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		UNPCManager* GetNPCManager() const { return NPCManager; }

		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		UNPCNetworkHandler* GetNPCNetworkHandler() const { return NPCNetworkHandler; }

		// Audio manager getter
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		UAudioManager* GetAudioManager() const { return AudioManager; }

		// Player stats manager getter (returns nullptr if not initialized)
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UPlayerStatsManager* GetPlayerStatsManager() const;

		// Dialogue manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UDialogueManager* GetDialogueManager() const;

		// Quest manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UQuestManager* GetQuestManager() const;

		// Equipment manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UEquipmentManager* GetEquipmentManager() const;

		// Vendor manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UVendorManager* GetVendorManager() const;

		// Repair manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class URepairManager* GetRepairManager() const;

		// Skill shop manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class USkillShopManager* GetSkillShopManager() const;

		// Trade manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UTradeManager* GetTradeManager() const;

		// Bestiary network handler
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UBestiaryNetworkHandler* GetBestiaryNetworkHandler() const;

		// Chat manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UChatManager* GetChatManager() const;

		// World Object manager
		UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Managers")
		class UWorldObjectManager* GetWorldObjectManager() const { return WorldObjectManager; }

	// Localization data asset (assign in Blueprint defaults ? all locale DataTables)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
	class ULocalizationDataAsset* LocalizationDataAsset;

	// Data tables and configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Instance Data")
	class UDataTable* MobDefinitionTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Instance Data")
	class UDataTable* NPCDefinitionTable;

	/** World Interactive Object definitions (FWIODefinitionRow).
	 *  Maps WIO slug to actor class and visual overrides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Instance Data|WIO")
	class UDataTable* WIODefinitionTable;

	/** Default actor class used when no DataTable row is found for a WIO slug.
	 *  Set this to your base BP_WorldInteractiveObjectActor blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Instance Data|WIO")
	TSubclassOf<class AWorldInteractiveObjectActor> WIODefaultActorClass;

		// Data table getters
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Data")
	UDataTable* GetMobDefinitionTable() const { return MobDefinitionTable; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Game Instance|Data")
	UDataTable* GetNPCDefinitionTable() const { return NPCDefinitionTable; }
};

// Forward declarations
class UPlayerManager;
class UMOBManager;
class USpawnZoneManager;
class UItemManager;
class UInventoryManager;
class UHarvestManager;
class UExperienceManager;
class UPlayerSkillManager;
class UCombatSystemManager;
class UTimeSyncService;
class UUIManager;
class UNPCManager;
class UNPCNetworkHandler;
class UChatNetworkHandler;
