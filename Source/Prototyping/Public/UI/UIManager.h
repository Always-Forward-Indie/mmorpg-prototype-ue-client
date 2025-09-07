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
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOTYPING_API UUIManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UUIManager();

	// Initialize the UI manager
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void Initialize(UInventoryManager* InInventoryManager, UHarvestManager* InHarvestManager = nullptr, UExperienceManager* InExperienceManager = nullptr, UPlayerSkillManager* InSkillManager = nullptr);

	// Initialize FCT Manager
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void Init(APlayerController* InPC, UCanvasPanel* InRootCanvas, 
		TSubclassOf<UDamageTextWidget> InDamageTextClass);

	// Create and setup all UI widgets
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateUIWidgets();

	// Initialize experience widget for specific character
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeExperienceWidget(int32 CharacterId);

	// Initialize skill widgets
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void InitializeSkillWidgets();

	// Toggle UI elements
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleSkillsPanel();

	// Handle input actions
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HandleInventoryToggle(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HandleSkillsPanelToggle(const FInputActionValue& Value);

	// Get UI widgets
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UInventoryWidget* GetInventoryWidget() const { return InventoryWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UHarvestProgressWidget* GetHarvestProgressWidget() const { return HarvestProgressWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UHarvestLootWidget* GetHarvestLootWidget() const { return HarvestLootWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UPlayerExperienceWidget* GetExperienceWidget() const { return ExperienceWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	USkillBarWidget* GetSkillBarWidget() const { return SkillBarWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UAvailableSkillsWidget* GetAvailableSkillsWidget() const { return AvailableSkillsWidget; }

	// Get FCT Manager
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UFloatingCombatTextManager* GetFCTManager() const { return FCTManager; }

	// Set current target for skill casting
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void SetSkillTarget(int32 TargetId, ECasterType TargetType);

	// Set PlayerController reference
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void SetPlayerController(APlayerController* InPlayerController);

protected:
	virtual void BeginPlay() override;

	// Create UI widgets
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateInventoryWidget();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateHarvestWidgets();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateExperienceWidget();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateSkillWidgets();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateGameVersionWidget();

protected:
	// Widget classes (set these in Blueprint or C++)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UHarvestProgressWidget> HarvestProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UHarvestLootWidget> HarvestLootWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UPlayerExperienceWidget> ExperienceWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<USkillBarWidget> SkillBarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UAvailableSkillsWidget> AvailableSkillsWidgetClass;

	//Version of game widget
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UUserWidget> GameVersionWidgetClass;

	// UI widgets
	UPROPERTY(BlueprintReadOnly, Category = "UI Manager")
	UInventoryWidget* InventoryWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI Manager")
	UHarvestProgressWidget* HarvestProgressWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI Manager")
	UHarvestLootWidget* HarvestLootWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI Manager")
	UPlayerExperienceWidget* ExperienceWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI Manager")
	USkillBarWidget* SkillBarWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI Manager")
	UAvailableSkillsWidget* AvailableSkillsWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI Manager")
	UUserWidget* GameVersionWidget;

	// FCT Manager
	UPROPERTY()
	UFloatingCombatTextManager* FCTManager;

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
	APlayerController* PlayerController;

	UPROPERTY()
	UCanvasPanel* RootCanvas;

	// UI settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	int32 InventoryRows = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	int32 InventoryColumns = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	int32 SkillBarSlots = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	bool bSkillsPanelVisible = false;

private:
	bool bIsInitialized = false;
};