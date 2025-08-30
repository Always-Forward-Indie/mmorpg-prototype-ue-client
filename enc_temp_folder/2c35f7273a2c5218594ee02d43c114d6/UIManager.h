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
class UInputAction;
class UFloatingCombatTextManager;
class UCanvasPanel;
class UDamageTextWidget;
class UHarvestProgressWidget;
class UHarvestLootWidget;

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
	void Initialize(UInventoryManager* InInventoryManager, UHarvestManager* InHarvestManager = nullptr);

	// Initialize FCT Manager
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void Init(APlayerController* InPC, UCanvasPanel* InRootCanvas, 
		TSubclassOf<UDamageTextWidget> InDamageTextClass);

	// Create and setup all UI widgets
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateUIWidgets();

	// Toggle inventory UI
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void ToggleInventory();

	// Handle input actions
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void HandleInventoryToggle(const FInputActionValue& Value);

	// Get UI widgets
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UInventoryWidget* GetInventoryWidget() const { return InventoryWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UHarvestProgressWidget* GetHarvestProgressWidget() const { return HarvestProgressWidget; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UHarvestLootWidget* GetHarvestLootWidget() const { return HarvestLootWidget; }

	// Get FCT Manager
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI Manager")
	UFloatingCombatTextManager* GetFCTManager() const { return FCTManager; }

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateGameVersionWidget();

protected:
	virtual void BeginPlay() override;

	// Create UI widgets
	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateInventoryWidget();

	UFUNCTION(BlueprintCallable, Category = "UI Manager")
	void CreateHarvestWidgets();

protected:
	// Widget classes (set these in Blueprint or C++)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UHarvestProgressWidget> HarvestProgressWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	TSubclassOf<UHarvestLootWidget> HarvestLootWidgetClass;

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
	APlayerController* PlayerController;

	UPROPERTY()
	UCanvasPanel* RootCanvas;

	// UI settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	int32 InventoryRows = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Manager")
	int32 InventoryColumns = 8;

private:
	bool bIsInitialized = false;
};