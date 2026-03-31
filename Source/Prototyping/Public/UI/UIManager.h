#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "InputActionValue.h"
#include "UI/InventoryWidget.h"
#include "UIManager.generated.h"

// Forward declarations
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
class UTradeWidget;
class UEquipmentWidget;

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

	// Initialize stats widget
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeStatsWidget(class UPlayerStatsManager* InStatsManager);

	// Initialize notification system
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeNotificationSystem(class UBestiaryNetworkHandler* InBestiaryHandler);

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
	UTradeWidget* GetTradeWidget() const { return TradeWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UEquipmentWidget* GetEquipmentWidget() const { return EquipmentWidget; }

	// Check if UIManager is fully initialized
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	bool IsInitialized() const { return bIsInitialized; }

	// Centralized cursor and input mode management
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void UpdateCursorAndInputMode();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	bool ShouldShowCursor() const;

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
	void CreateExperienceWidget();
	void CreateSkillWidgets();
	void CreateGameVersionWidget();

	// Event handlers for widget visibility changes
	UFUNCTION()
	void OnAvailableSkillsVisibilityChanged(bool bIsVisible);

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
	void OnTradeVisibilityChanged(bool bIsVisible);

	UFUNCTION()
	void OnEquipmentVisibilityChanged(bool bIsVisible);

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
	TSubclassOf<UAvailableSkillsWidget> AvailableSkillsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UUserWidget> GameVersionWidgetClass;

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
	TSubclassOf<UTradeWidget> TradeWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UEquipmentWidget> EquipmentWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UDeathScreenWidget> DeathScreenWidgetClass;

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
	UAvailableSkillsWidget* AvailableSkillsWidget;

	UPROPERTY()
	UUserWidget* GameVersionWidget;

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
	UTradeWidget* TradeWidget;

	UPROPERTY()
	UEquipmentWidget* EquipmentWidget;

	UPROPERTY()
	UDeathScreenWidget* DeathScreenWidget;

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
	bool bTradeVisible;
	bool bEquipmentVisible;

	UFUNCTION()
	void OnMenuBarBestiaryClicked();

	UFUNCTION()
	void HandleGameMenuResumeClicked();

public:
	/** Trigger a camera shake effect for combat feedback. Intensity in [0..1]. */
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void PlayCombatCameraShake(float Intensity = 1.0f);

	/** Show/update the mob target frame (health bar, name, level, icon). */
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ShowMobTargetFrame(const FString& MobSlug, const FString& MobName, int32 MobLevel, int32 CurrentHP, int32 MaxHP, bool bIsAggro, UTexture2D* MobIcon = nullptr);

	/** Hide the mob target frame. */
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HideMobTargetFrame();
};