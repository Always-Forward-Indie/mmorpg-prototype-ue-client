#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "Data/WIODataStructs.h"
#include "Gameplay/Interaction/IWorldInteractable.h"
#include "InputActionValue.h"
#include "UI/InventoryWidget.h"
#include "Camera/CameraShakeBase.h"
#include "UIManager.generated.h"

// Forward declarations
class UCombatScreenFlashWidget;
class UInventoryManager;
class UHarvestManager;
class UExperienceManager;
class UPlayerSkillManager;
class UInputAction;
class UFloatingCombatTextManager;
class UCanvasPanel;
class UDamageTextWidget;
class UHarvestProgressWidget;
class UHarvestLootWidget;
class UPlayerExperienceWidget;
class USkillBarWidget;
class UAvailableSkillsWidget;
class UPlayerInterfaceWidget;
class UDamageCanvasWidget;
class UNameplateManager;
class UDialogueWidget;
class UQuestJournalWidget;
class UQuestTrackerWidget;
class UVendorShopWidget;
class UDeathScreenWidget;
class URepairShopWidget;
class USkillShopWidget;
class UTradeWidget;
class UEquipmentWidget;
class UPlayerStatsWidget;
class UBestiaryWidget;
class UChatWidget;
class UPlayerStatsManager;
class UBestiaryNetworkHandler;
class UChatManager;
class UGameMenuWidget;
class UGameMenuBarWidget;
class UAudioSettingsWidget;
class UW_SettingsWidget;
class UAudioManager;
class UNotificationToastWidget;
class UNotificationZoneBannerWidget;
class UNotificationScreenCenterWidget;
class UNotificationAtmosphereWidget;
class UWorldNotificationManager;
class UGameVersionWidget;
class UTitlesWidget;
class UReputationWidget;
class UTitleManager;
class UTitleNetworkHandler;
class UReputationManager;
class UWIOInteractionPromptWidget;
class UWIOChannelBarWidget;
class UWorldObjectManager;
class UIdleWarningWidget;

// Delegate for UI Manager initialization completion
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIManagerInitialized);

/**
 * UI Manager component that handles all UI elements for the player
 * Now uses PlayerInterfaceWidget for better UI organization
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UUIManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UUIManager();

	// Event broadcast when UIManager is fully initialized
	UPROPERTY(BlueprintAssignable, Category = "UI Manager|Events")
	FOnUIManagerInitialized OnUIManagerInitialized;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Initialize the UI manager with required managers
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void Initialize(UInventoryManager* InInventoryManager, UHarvestManager* InHarvestManager,
		UExperienceManager* InExperienceManager, UPlayerSkillManager* InSkillManager);

	// Initialize floating combat text system with the new damage canvas
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitFTCManager(APlayerController* InPC);


	// Initialize experience widget with character ID (called after character login)
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeExperienceWidget(int32 CharacterId);

	// Initialize skill widgets after skill manager is ready
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeSkillWidgets();

	// Toggle functions for UI panels
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleSkillsPanel();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleGameMenu();

	// Initialize dialogue and quest widgets
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeDialogueAndQuestWidgets(class UDialogueManager* InDialogueManager, class UQuestManager* InQuestManager);

	// Initialize item system widgets (equipment, vendor, repair, trade)
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeItemSystemWidgets(class UEquipmentManager* InEquipmentManager, class UVendorManager* InVendorManager, class URepairManager* InRepairManager, class UTradeManager* InTradeManager);
	void InitializeSkillShopWidget(class USkillShopManager* InSkillShopManager);

	// Initialize stats widget
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeStatsWidget(class UPlayerStatsManager* InStatsManager);

	// Initialize titles window
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeTitlesWidget(class UTitleManager* InTitleManager, class UTitleNetworkHandler* InTitleHandler, int32 InCharacterId);

	// Initialize reputation window
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeReputationWidget(class UReputationManager* InReputationManager);

	// Initialize emote window
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeEmoteListWidget(class UEmoteManager* InEmoteManager, class UEmoteNetworkHandler* InEmoteHandler, int32 InCharacterId);

	// Initialize notification system
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeNotificationSystem(class UBestiaryNetworkHandler* InBestiaryHandler);

	// Initialize WIO (World Interactive Objects) widgets
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeWIOWidgets(UWorldObjectManager* InWorldObjectManager);

	// Show WIO interaction prompt for a specific world object
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowWIOInteractionPrompt(int32 ObjectId);

	// Hide WIO interaction prompt
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HideWIOInteractionPrompt();

	// Show WIO channel bar
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowWIOChannelBar(int32 ObjectId, float Duration);

	// Hide WIO channel bar
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HideWIOChannelBar();

	// Idle warning widget control (called by IdleTimeoutManager)
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowIdleWarning(int32 TotalSecondsRemaining);

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HideIdleWarning();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void UpdateIdleCountdown(int32 SecondsRemaining);

	// Input action handlers
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HandleInventoryToggle(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HandleSkillsPanelToggle(const FInputActionValue& Value);

	// Set current skill target for skill casting
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void SetSkillTarget(int32 TargetId, ECasterType TargetType);

	// Set player controller reference
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void SetPlayerController(APlayerController* InPlayerController);

	// Public getters for other systems to access UI widgets
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UInventoryWidget* GetInventoryWidget() const { return InventoryWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UHarvestProgressWidget* GetHarvestProgressWidget() const { return HarvestProgressWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UHarvestLootWidget* GetHarvestLootWidget() const { return HarvestLootWidget; }

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowInteractionHint(EInteractableType Type);

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HideInteractionHint();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowProximityHint(EInteractableType Type);

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ClearProximityHint();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	USkillBarWidget* GetSkillBarWidget() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UAvailableSkillsWidget* GetAvailableSkillsWidget() const { return AvailableSkillsWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UFloatingCombatTextManager* GetFCTManager() const { return FCTManager; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UPlayerInterfaceWidget* GetPlayerInterfaceWidget() const { return PlayerInterfaceWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UDamageCanvasWidget* GetDamageCanvasWidget() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UPlayerExperienceWidget* GetPlayerExperienceWidget() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UDialogueWidget* GetDialogueWidget() const { return DialogueWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UQuestJournalWidget* GetQuestJournalWidget() const { return QuestJournalWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UQuestTrackerWidget* GetQuestTrackerWidget() const { return QuestTrackerWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UVendorShopWidget* GetVendorShopWidget() const { return VendorShopWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	URepairShopWidget* GetRepairShopWidget() const { return RepairShopWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	USkillShopWidget* GetSkillShopWidget() const { return SkillShopWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UTradeWidget* GetTradeWidget() const { return TradeWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UEquipmentWidget* GetEquipmentWidget() const { return EquipmentWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UPlayerStatsWidget* GetPlayerStatsWidget() const { return PlayerStatsWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UTitlesWidget* GetTitlesWidget() const { return TitlesWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UReputationWidget* GetReputationWidget() const { return ReputationWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UBestiaryWidget* GetBestiaryWidget() const { return BestiaryWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	class UEmoteListWidget* GetEmoteListWidget() const { return EmoteListWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UChatWidget* GetChatWidget() const { return ChatWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UGameMenuBarWidget* GetGameMenuBarWidget() const { return GameMenuBarWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UGameMenuWidget* GetGameMenuWidget() const { return GameMenuWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UAudioSettingsWidget* GetAudioSettingsWidget() const { return AudioSettingsWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UW_SettingsWidget* GetGameSettingsWidget() const { return GameSettingsWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UWorldNotificationManager* GetWorldNotificationManager() const { return WorldNotificationManager; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UNotificationToastWidget* GetNotificationToastWidget() const { return NotificationToastWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UWIOInteractionPromptWidget* GetWIOInteractionPromptWidget() const { return WIOInteractionPromptWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UWIOChannelBarWidget* GetWIOChannelBarWidget() const { return WIOChannelBarWidget; }

	// Check if UIManager is fully initialized
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	bool IsInitialized() const { return bIsInitialized; }

	// Centralized cursor and input mode management
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void UpdateCursorAndInputMode();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	bool ShouldShowCursor() const;

	/**
	 * Returns true only when an actual UI panel (inventory, vendor, dialogue, etc.) is open
	 * and consuming cursor input.  Does NOT include bAltCursorActive so that cursor world
	 * interaction stays functional even while the cursor is always shown.
	 * Use this check for: IsUIBlockingInteraction, ApplyMouseCaptureIfNoUIOpen, scroll guard.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	bool HasUIWindowOpen() const;

	/**
	 * Returns true when a UI window is open AND the cursor is hovering over
	 * an interactable child widget (button, slot, input field, etc.).
	 * Empty/transparent areas of windows (SelfHitTestInvisible) pass through.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	bool HasCursorOverWindowContent() const;

	/**
	 * Returns true only when a modal window is open that should fully block world interaction
	 * (dialogue, trade, harvest loot, game menu). Non-modal windows like inventory, stats,
	 * skills, etc. do NOT block world interaction — UMG hit-testing handles click-through
	 * for transparent areas naturally.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	bool HasModalWindowOpen() const;

	/**
	 * Returns the NPC id of the currently-open NPC interaction window
	 * (dialogue, vendor shop, repair shop, or skill shop). Returns 0 if none.
	 */
	int32 GetActiveInteractionNpcId() const;

	/**
	 * Force-closes all NPC-related UI windows (dialogue widget + all shop widgets).
	 * Sends the dialogueClose packet to the server when a dialogue session is active.
	 * Call this when the player walks away from a NPC or starts interacting with another.
	 */
	void ForceCloseAllNPCWindows(class UDialogueManager* DlgMgr);

	// Additional UI toggles
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleEquipment();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleAltCursor();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void TogglePlayerStats();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleBestiary();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleTitles();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleReputation();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleEmoteList();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleQuestJournal();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowDeathScreen(int32 RespawnTimeSec);

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HideDeathScreen();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void UpdateMobTargetFrameHP(int32 CurrentHP, int32 MaxHP);

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowHealScreenFlash();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowDamageScreenFlash();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void SetLowHealthWarning(bool bActive);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UNameplateManager* GetNameplateManager() const { return NameplateManager; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Manager")
	TSubclassOf<class UDamageTextWidget> DamageTextWidgetClass;

protected:
	// Widget creation methods
	void CreateUIWidgets();
	void CreatePlayerInterfaceWidget();
	void CreateInventoryWidget();
	void CreateHarvestWidgets();
	void CreateInteractionHintWidget();
	void CreateExperienceWidget();
	void CreateSkillWidgets();
	void CreateGameVersionWidget();
	void CreateGameMenuBarWidget();
	void CreateGameMenuWidget();

	// Event handlers for widget visibility changes
	UFUNCTION()
	void OnAvailableSkillsVisibilityChanged(bool bIsVisible);

	// Called by PlayerInterfaceWidget on its first valid tick � relays OnUIManagerInitialized
	UFUNCTION()
	void HandlePlayerInterfaceReady();

	UFUNCTION()
	void OnInventoryVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnHarvestLootVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnDialogueVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnQuestJournalVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnVendorShopVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnRepairShopVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnSkillShopVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnTradeVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnEquipmentVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnPlayerStatsVisibilityChanged();

	UFUNCTION()
	void OnTitlesVisibilityChanged();

	UFUNCTION()
	void OnReputationVisibilityChanged();

	UFUNCTION()
	void OnEmoteListVisibilityChanged();

	UFUNCTION()
	void OnBestiaryVisibilityChanged(bool bIsVisible);

protected:
	// Widget class references (set in Blueprint)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UPlayerInterfaceWidget> PlayerInterfaceWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UHarvestProgressWidget> HarvestProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UHarvestLootWidget> HarvestLootWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<class UInteractionHintWidget> InteractionHintWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UAvailableSkillsWidget> AvailableSkillsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UGameVersionWidget> GameVersionWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UDialogueWidget> DialogueWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UQuestJournalWidget> QuestJournalWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UQuestTrackerWidget> QuestTrackerWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UVendorShopWidget> VendorShopWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<URepairShopWidget> RepairShopWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<USkillShopWidget> SkillShopWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UTradeWidget> TradeWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UEquipmentWidget> EquipmentWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UDeathScreenWidget> DeathScreenWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UPlayerStatsWidget> PlayerStatsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UTitlesWidget> TitlesWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UReputationWidget> ReputationWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UBestiaryWidget> BestiaryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<class UEmoteListWidget> EmoteListWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UChatWidget> ChatWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UGameMenuBarWidget> GameMenuBarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UGameMenuWidget> GameMenuWidgetClass;

	/** URL opened when the player clicks Bug Report in the game menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	FString BugReportUrl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UAudioSettingsWidget> AudioSettingsWidgetClass;

	/** Full tabbed settings window (WBP_Settings, subclass of UW_SettingsWidget).
	 *  Opened when the player clicks Settings in the in-game pause menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UW_SettingsWidget> GameSettingsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UIdleWarningWidget> IdleWarningWidgetClass;

	// Notification widget classes (world_notification visual layer)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Notification Widgets")
	TSubclassOf<UNotificationToastWidget> NotificationToastWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Notification Widgets")
	TSubclassOf<UNotificationZoneBannerWidget> NotificationZoneBannerWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Notification Widgets")
	TSubclassOf<UNotificationScreenCenterWidget> NotificationScreenCenterWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Notification Widgets")
	TSubclassOf<UNotificationAtmosphereWidget> NotificationAtmosphereWidgetClass;

	// WIO widget classes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UWIOInteractionPromptWidget> WIOInteractionPromptWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UWIOChannelBarWidget> WIOChannelBarWidgetClass;

	// Configuration properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Configuration")
	int32 InventoryRows = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Configuration")
	int32 InventoryColumns = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Configuration")
	int32 SkillBarSlots = 10;

	// Widget instances
	UPROPERTY()
	UPlayerInterfaceWidget* PlayerInterfaceWidget;

	UPROPERTY()
	UInventoryWidget* InventoryWidget;

	UPROPERTY()
	UHarvestProgressWidget* HarvestProgressWidget;

	UPROPERTY()
	UHarvestLootWidget* HarvestLootWidget;

	UPROPERTY()
	class UInteractionHintWidget* InteractionHintWidget;

	bool bHoverHintActive     = false;
	bool bProximityHintActive = false;

	UPROPERTY()
	UAvailableSkillsWidget* AvailableSkillsWidget;

	UPROPERTY()
	UGameVersionWidget* GameVersionWidget;

	UPROPERTY()
	UDialogueWidget* DialogueWidget;

	UPROPERTY()
	UQuestJournalWidget* QuestJournalWidget;

	UPROPERTY()
	UQuestTrackerWidget* QuestTrackerWidget;

	UPROPERTY()
	UVendorShopWidget* VendorShopWidget;

	UPROPERTY()
	URepairShopWidget* RepairShopWidget;

	UPROPERTY()
	USkillShopWidget* SkillShopWidget;

	UPROPERTY()
	UTradeWidget* TradeWidget;

	UPROPERTY()
	UEquipmentWidget* EquipmentWidget;

	UPROPERTY()
	UDeathScreenWidget* DeathScreenWidget;

	UPROPERTY()
	UPlayerStatsWidget* PlayerStatsWidget;

	UPROPERTY()
	UTitlesWidget* TitlesWidget = nullptr;

	UPROPERTY()
	UReputationWidget* ReputationWidget = nullptr;

	UPROPERTY()
	class UEmoteListWidget* EmoteListWidget = nullptr;

	UPROPERTY()
	UBestiaryWidget* BestiaryWidget;

	UPROPERTY()
	UChatWidget* ChatWidget;

	UPROPERTY()
	UGameMenuBarWidget* GameMenuBarWidget;

	UPROPERTY()
	UGameMenuWidget* GameMenuWidget;

	UPROPERTY()
	UAudioSettingsWidget* AudioSettingsWidget;

	UPROPERTY()
	UW_SettingsWidget* GameSettingsWidget;

	// Notification widget instances
	UPROPERTY()
	UNotificationToastWidget* NotificationToastWidget = nullptr;

	UPROPERTY()
	UNotificationZoneBannerWidget* NotificationZoneBannerWidget = nullptr;

	UPROPERTY()
	UNotificationScreenCenterWidget* NotificationScreenCenterWidget = nullptr;

	UPROPERTY()
	UNotificationAtmosphereWidget* NotificationAtmosphereWidget = nullptr;

	UPROPERTY()
	UWorldNotificationManager* WorldNotificationManager = nullptr;

	// Idle warning widget instance
	UPROPERTY()
	UIdleWarningWidget* IdleWarningWidget = nullptr;

	// WIO widget instances
	UPROPERTY()
	UWIOInteractionPromptWidget* WIOInteractionPromptWidget = nullptr;

	UPROPERTY()
	UWIOChannelBarWidget* WIOChannelBarWidget = nullptr;

	// Manager references
	UPROPERTY()
	UInventoryManager* InventoryManager;

	UPROPERTY()
	UHarvestManager* HarvestManager;

	UPROPERTY()
	UExperienceManager* ExperienceManager;

	UPROPERTY()
	UPlayerSkillManager* SkillManager;

	UPROPERTY()
	UFloatingCombatTextManager* FCTManager;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - FCT|Distance")
	float FCTMaxVisibleDistanceCm = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - FCT|Distance")
	float FCTFadeStartDistanceCm = 3000.0f;

	UPROPERTY()
	UNameplateManager* NameplateManager = nullptr;

	// Player controller reference
	UPROPERTY()
	APlayerController* PlayerController;

	// State tracking
	bool bIsInitialized;

	// Pending death screen: set to true if ShowDeathScreen() is called before
	// PlayerController is assigned (e.g. character spawns dead at login).
	bool bPendingDeathScreen = false;
	int32 PendingDeathScreenDebt = 0;

	// Widget visibility tracking for cursor management
	bool bInventoryVisible;
	bool bSkillsPanelVisible;
	bool bHarvestLootVisible;
	bool bDialogueVisible;
	bool bQuestJournalVisible;
	bool bVendorShopVisible;
	bool bRepairShopVisible;
	bool bSkillShopVisible;
	bool bTradeVisible;
	bool bEquipmentVisible;
	bool bPlayerStatsVisible;
	bool bBestiaryVisible;
	bool bTitlesVisible = false;
	bool bReputationVisible = false;
	bool bEmoteListVisible = false;
	bool bAltCursorActive;
	bool bGameMenuVisible;

public:
	// Internal delegate handlers (must be public for AddDynamic)
	UFUNCTION() void OnMenuBarBestiaryClicked();
	UFUNCTION() void OnMenuBarTitlesClicked();
	UFUNCTION() void OnMenuBarReputationClicked();
	UFUNCTION() void OnMenuBarEmoteClicked();
	UFUNCTION() void HandleGameMenuResumeClicked();
	UFUNCTION() void HandleSettingsClicked();
	UFUNCTION() void HandleAudioSettingsClosed();
	UFUNCTION() void HandleExitToLoginClicked();
	UFUNCTION() void HandleExitToDesktopClicked();
	UFUNCTION() void HandleBugReportClicked();
	UFUNCTION() void OnMenuBarInventoryClicked();
	UFUNCTION() void OnMenuBarEquipmentClicked();
	UFUNCTION() void OnMenuBarQuestJournalClicked();
	UFUNCTION() void OnMenuBarSkillsClicked();
	UFUNCTION() void OnMenuBarStatsClicked();
	UFUNCTION() void OnMenuBarMenuClicked();

protected:

public:
	/** Camera shake class used for hit feedback. Assign in the Blueprint default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Combat Effects")
	TSubclassOf<UCameraShakeBase> CombatCameraShakeClass;

	/** Widget class used for the full-screen damage/heal flash overlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Combat Effects")
	TSubclassOf<UCombatScreenFlashWidget> CombatScreenFlashWidgetClass;

	/** Trigger a camera shake effect for combat feedback. Intensity in [0..1]. */
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void PlayCombatCameraShake(float Intensity = 1.0f);

	/** Show/update the mob target frame (health bar, name, level, icon). */
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowMobTargetFrame(const FString& MobSlug, const FString& MobName, int32 MobLevel, int32 CurrentHP, int32 MaxHP, bool bIsAggro, UTexture2D* MobIcon = nullptr);

	/** Hide the mob target frame. */
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HideMobTargetFrame();

	// WIO delegate handlers (bound by InitializeWIOWidgets)
	UFUNCTION()
	void HandleWIOInteractResult(const FWIOInteractResult& Result);

	UFUNCTION()
	void HandleWIOChannelCancelled(int32 ObjectId);

private:
	/** Ensures the flash widget is created and in the viewport. */
	void EnsureFlashWidget();

	/** Live flash widget instance (created lazily). */
	UPROPERTY()
	UCombatScreenFlashWidget* CombatScreenFlashWidget = nullptr;

	/** Recursively checks if any child of Widget is under MousePos and Visible. */
	static bool DoesWidgetTreeHaveHoveredChild(UWidget* Widget, const FVector2f& MousePos);
};