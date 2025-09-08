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

/**
 * UI Manager component that handles all UI elements for the player
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UUIManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UUIManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Initialize the UI manager with required managers
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void Initialize(UInventoryManager* InInventoryManager, UHarvestManager* InHarvestManager, 
		UExperienceManager* InExperienceManager, UPlayerSkillManager* InSkillManager);

	// Initialize floating combat text system
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void Init(APlayerController* InPC, UCanvasPanel* InRootCanvas, 
		TSubclassOf<UDamageTextWidget> InDamageTextClass);

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
	USkillBarWidget* GetSkillBarWidget() const { return SkillBarWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UAvailableSkillsWidget* GetAvailableSkillsWidget() const { return AvailableSkillsWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UFloatingCombatTextManager* GetFCTManager() const { return FCTManager; }

protected:
	// Widget creation methods
	void CreateUIWidgets();
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

	// Centralized cursor and input mode management
	void UpdateCursorAndInputMode();
	bool ShouldShowCursor() const;

protected:
	// Widget class references (set in Blueprint)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UHarvestProgressWidget> HarvestProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UHarvestLootWidget> HarvestLootWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UPlayerExperienceWidget> ExperienceWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<USkillBarWidget> SkillBarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UAvailableSkillsWidget> AvailableSkillsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Widget Classes")
	TSubclassOf<UUserWidget> GameVersionWidgetClass;

	// Configuration properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Configuration")
	int32 InventoryRows = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Configuration")
	int32 InventoryColumns = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager - Configuration")
	int32 SkillBarSlots = 10;

	// Widget instances
	UPROPERTY()
	UInventoryWidget* InventoryWidget;

	UPROPERTY()
	UHarvestProgressWidget* HarvestProgressWidget;

	UPROPERTY()
	UHarvestLootWidget* HarvestLootWidget;

	UPROPERTY()
	UPlayerExperienceWidget* ExperienceWidget;

	UPROPERTY()
	USkillBarWidget* SkillBarWidget;

	UPROPERTY()
	UAvailableSkillsWidget* AvailableSkillsWidget;

	UPROPERTY()
	UUserWidget* GameVersionWidget;

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

	// Player controller reference
	UPROPERTY()
	APlayerController* PlayerController;

	// Root canvas for UI elements
	UPROPERTY()
	UCanvasPanel* RootCanvas;

	// State tracking
	bool bIsInitialized;
	
	// Widget visibility tracking for cursor management
	bool bInventoryVisible;
	bool bSkillsPanelVisible;
	bool bHarvestLootVisible;
};