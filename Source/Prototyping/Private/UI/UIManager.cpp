#include "UI/UIManager.h"
#include "UI/InventoryWidget.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "UI/SkillBarWidget.h"
#include "UI/AvailableSkillsWidget.h"
#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MyGameInstance.h"

UUIManager::UUIManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	InventoryWidget = nullptr;
	HarvestProgressWidget = nullptr;
	HarvestLootWidget = nullptr;
	ExperienceWidget = nullptr;
	SkillBarWidget = nullptr;
	AvailableSkillsWidget = nullptr;
	InventoryManager = nullptr;
	HarvestManager = nullptr;
	ExperienceManager = nullptr;
	SkillManager = nullptr;
	FCTManager = nullptr;
	PlayerController = nullptr;
	RootCanvas = nullptr;
	GameVersionWidget = nullptr;
	bIsInitialized = false;
	bSkillsPanelVisible = false;
}

void UUIManager::BeginPlay()
{
	Super::BeginPlay();
	
	// UI creation is handled manually through Initialize() or CreateUIWidgets()
	// This allows for better control over when UI is created
}

void UUIManager::Initialize(UInventoryManager* InInventoryManager, UHarvestManager* InHarvestManager, UExperienceManager* InExperienceManager, UPlayerSkillManager* InSkillManager)
{
	if (!InInventoryManager)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Cannot initialize with null InventoryManager"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("UIManager: Starting initialization"));

	InventoryManager = InInventoryManager;
	HarvestManager = InHarvestManager;
	ExperienceManager = InExperienceManager;
	SkillManager = InSkillManager;
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Managers set - InventoryManager: %s, HarvestManager: %s, ExperienceManager: %s, SkillManager: %s"), 
		InventoryManager ? TEXT("Valid") : TEXT("NULL"), 
		HarvestManager ? TEXT("Valid") : TEXT("NULL"),
		ExperienceManager ? TEXT("Valid") : TEXT("NULL"),
		SkillManager ? TEXT("Valid") : TEXT("NULL"));
	
	// Create UI widgets
	CreateUIWidgets();
	
	bIsInitialized = true;
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Successfully initialized"));
}

void UUIManager::Init(APlayerController* InPC, UCanvasPanel* InRootCanvas, 
	TSubclassOf<UDamageTextWidget> InDamageTextClass)
{
	UE_LOG(LogTemp, Warning, TEXT("UIManager::Init called with PC: %s, Canvas: %s, DamageTextClass: %s"), 
		InPC ? TEXT("Valid") : TEXT("NULL"),
		InRootCanvas ? TEXT("Valid") : TEXT("NULL"), 
		InDamageTextClass ? TEXT("Valid") : TEXT("NULL"));

	PlayerController = InPC;
	RootCanvas = InRootCanvas;

	if (PlayerController && RootCanvas && InDamageTextClass)
	{
		// Create FCT Manager
		FCTManager = NewObject<UFloatingCombatTextManager>(this);
		if (FCTManager)
		{
			FCTManager->Init(RootCanvas, PlayerController, InDamageTextClass);
			
			UE_LOG(LogTemp, Warning, TEXT("UIManager: FCT Manager initialized successfully"));
			
			// Verify the FCTManager is properly set up
			if (FCTManager->GetRootCanvas() && FCTManager->GetPlayerController() && FCTManager->GetDamageTextClass())
			{
				UE_LOG(LogTemp, Warning, TEXT("UIManager: FCT Manager validation successful - all components ready"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UIManager: FCT Manager validation failed after init"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create FCTManager object"));
		}
		
		// Store PlayerController reference for later use
		UE_LOG(LogTemp, Warning, TEXT("UIManager: PlayerController reference stored for UI management"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Cannot initialize FCT Manager - missing required parameters"));
	}
}

void UUIManager::CreateUIWidgets()
{
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Cannot create UI widgets - no world context"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("UIManager: Starting UI widgets creation"));

	// Create inventory widget
	CreateInventoryWidget();
	
	// Create harvest widgets
	CreateHarvestWidgets();

	// Create experience widget
	CreateExperienceWidget();

	// Create skill widgets
	CreateSkillWidgets();

	// Create game version widget
	CreateGameVersionWidget();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: All UI widgets created successfully"));
}

void UUIManager::CreateInventoryWidget()
{
	if (!InventoryWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: InventoryWidgetClass is not set"));
		return;
	}

	if (InventoryWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Inventory widget already exists, removing old one"));
		InventoryWidget->RemoveFromParent();
		InventoryWidget = nullptr;
	}

	// Create the widget
	InventoryWidget = CreateWidget<UInventoryWidget>(GetWorld(), InventoryWidgetClass);
	if (!InventoryWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create inventory widget"));
		return;
	}

	// Set inventory size
	InventoryWidget->SetInventorySize(InventoryRows, InventoryColumns);

	// Add to viewport with highest Z-Order for overlays
	InventoryWidget->AddToViewport(100);

	// Initialize with inventory manager
	if (InventoryManager)
	{
		InventoryManager->SetInventoryUIWidget(InventoryWidget);
	}

	// Hide initially
	InventoryWidget->SetInventoryVisible(false);

	UE_LOG(LogTemp, Warning, TEXT("UIManager: Inventory widget created and configured"));
}

void UUIManager::CreateHarvestWidgets()
{
	// Create harvest progress widget
	if (HarvestProgressWidgetClass)
	{
		if (HarvestProgressWidget)
		{
			HarvestProgressWidget->RemoveFromParent();
			HarvestProgressWidget = nullptr;
		}

		HarvestProgressWidget = CreateWidget<UHarvestProgressWidget>(GetWorld(), HarvestProgressWidgetClass);
		if (HarvestProgressWidget)
		{
			HarvestProgressWidget->AddToViewport();
			
			// Ensure widget is hidden initially - force visibility to hidden
			HarvestProgressWidget->SetVisibility(ESlateVisibility::Hidden);
			
			// Connect to harvest manager
			if (HarvestManager)
			{
				HarvestManager->SetHarvestProgressWidget(HarvestProgressWidget);
			}
			
			UE_LOG(LogTemp, Warning, TEXT("UIManager: Harvest progress widget created and forcibly hidden"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create harvest progress widget"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: HarvestProgressWidgetClass is not set"));
	}

	// Create harvest loot widget
	if (HarvestLootWidgetClass)
	{
		if (HarvestLootWidget)
		{
			HarvestLootWidget->RemoveFromParent();
			HarvestLootWidget = nullptr;
		}

		HarvestLootWidget = CreateWidget<UHarvestLootWidget>(GetWorld(), HarvestLootWidgetClass);
		if (HarvestLootWidget)
		{
			HarvestLootWidget->AddToViewport();
			
			// Ensure widget is hidden initially - force visibility to hidden
			HarvestLootWidget->SetVisibility(ESlateVisibility::Hidden);
			
			// Connect to harvest manager
			if (HarvestManager)
			{
				HarvestManager->SetHarvestLootWidget(HarvestLootWidget);
			}
			
			UE_LOG(LogTemp, Warning, TEXT("UIManager: Harvest loot widget created and forcibly hidden"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create harvest loot widget"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: HarvestLootWidgetClass is not set"));
	}
}

void UUIManager::CreateGameVersionWidget()
{
	if (!GameVersionWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: GameVersionWidgetClass is not set"));
		return;
	}
	if (GameVersionWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Game version widget already exists, removing old one"));
		GameVersionWidget->RemoveFromParent();
		GameVersionWidget = nullptr;
	}
	// Create the widget
	GameVersionWidget = CreateWidget<UUserWidget>(GetWorld(), GameVersionWidgetClass);
	if (!GameVersionWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create game version widget"));
		return;
	}
	// Add to viewport with very low Z-Order (background HUD element)
	GameVersionWidget->AddToViewport(1);
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Game version widget created and added to viewport"));
}

void UUIManager::ToggleInventory()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Cannot toggle inventory - not initialized"));
		return;
	}

	if (InventoryManager)
	{
		InventoryManager->ToggleInventoryUI();
		UE_LOG(LogTemp, Log, TEXT("UIManager: Toggled inventory UI"));
	}
}

void UUIManager::HandleInventoryToggle(const FInputActionValue& Value)
{
	if (Value.Get<bool>())
	{
		ToggleInventory();
	}
}

void UUIManager::CreateExperienceWidget()
{
	if (!ExperienceWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: ExperienceWidgetClass is not set"));
		return;
	}

	if (ExperienceWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Experience widget already exists, removing old one"));
		ExperienceWidget->RemoveFromParent();
		ExperienceWidget = nullptr;
	}

	// Create the widget
	ExperienceWidget = CreateWidget<UPlayerExperienceWidget>(GetWorld(), ExperienceWidgetClass);
	if (!ExperienceWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create experience widget"));
		return;
	}

	// Add to viewport with low Z-Order (HUD element)
	ExperienceWidget->AddToViewport(5);

	// Widget will be initialized later when character ID is available
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Experience widget created (will be initialized when character ID is available)"));
}

void UUIManager::InitializeExperienceWidget(int32 CharacterId)
{
	if (!ExperienceWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Experience widget not created yet"));
		return;
	}

	if (!ExperienceManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: ExperienceManager not available for experience widget initialization"));
		return;
	}

	if (CharacterId <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Invalid character ID for experience widget initialization: %d"), CharacterId);
		return;
	}

	// Initialize the widget with the character ID
	ExperienceWidget->InitializeWidget(ExperienceManager, CharacterId);
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Experience widget initialized for character %d"), CharacterId);
}

void UUIManager::CreateSkillWidgets()
{
	if (!SkillManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: SkillManager not available, skipping skill widgets creation"));
		return;
	}

	// Create skill bar widget
	if (SkillBarWidgetClass)
	{
		if (SkillBarWidget)
		{
			SkillBarWidget->RemoveFromParent();
			SkillBarWidget = nullptr;
		}

		SkillBarWidget = CreateWidget<USkillBarWidget>(GetWorld(), SkillBarWidgetClass);
		if (SkillBarWidget)
		{
			SkillBarWidget->AddToViewport(10); // Lower Z-Order for skill bar
			
			// Get game instance for initialization
			if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
			{
				SkillBarWidget->Initialize(GameInstance);
				SkillBarWidget->CreateSkillSlots(SkillBarSlots);
				UE_LOG(LogTemp, Warning, TEXT("UIManager: Skill bar widget created and initialized"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to get GameInstance for skill bar widget"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create skill bar widget"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: SkillBarWidgetClass is not set"));
	}

	// Create available skills widget
	if (AvailableSkillsWidgetClass)
	{
		if (AvailableSkillsWidget)
		{
			AvailableSkillsWidget->RemoveFromParent();
			AvailableSkillsWidget = nullptr;
		}

		AvailableSkillsWidget = CreateWidget<UAvailableSkillsWidget>(GetWorld(), AvailableSkillsWidgetClass);
		if (AvailableSkillsWidget)
		{
			// Add to viewport with higher Z-Order than skill bar but lower than inventory
			AvailableSkillsWidget->AddToViewport(50);
			
			// Initially hide the available skills panel
			AvailableSkillsWidget->SetVisibility(ESlateVisibility::Hidden);
			
			// Get game instance for initialization
			if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
			{
				AvailableSkillsWidget->Initialize(GameInstance);
				UE_LOG(LogTemp, Warning, TEXT("UIManager: Available skills widget created and initialized"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to get GameInstance for available skills widget"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create available skills widget"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: AvailableSkillsWidgetClass is not set"));
	}
}

void UUIManager::InitializeSkillWidgets()
{
	if (!SkillManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: SkillManager not available for skill widgets initialization"));
		return;
	}

	// Both widgets should already be initialized in CreateSkillWidgets
	if (SkillBarWidget)
	{
		// Refresh skill bar to show current skills
		SkillBarWidget->RefreshAllSlots();
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Skill bar widget refreshed"));
	}

	if (AvailableSkillsWidget)
	{
		// Refresh available skills list
		AvailableSkillsWidget->RefreshSkillList();
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Available skills widget refreshed"));
	}
}

void UUIManager::ToggleSkillsPanel()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Cannot toggle skills panel - not initialized"));
		return;
	}

	if (!AvailableSkillsWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Available skills widget not created"));
		return;
	}

	bSkillsPanelVisible = !bSkillsPanelVisible;
	
	if (bSkillsPanelVisible)
	{
		AvailableSkillsWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AvailableSkillsWidget->RefreshSkillList();
		
		// Set focus and enable cursor when opening skills panel
		if (PlayerController)
		{
			// Show cursor for UI interaction
			PlayerController->bShowMouseCursor = true;
			PlayerController->bEnableClickEvents = true;
			PlayerController->bEnableMouseOverEvents = true;
			
			// Set input mode to allow UI input
			FInputModeGameAndUI InputMode;
			//InputMode.SetWidgetToFocus(AvailableSkillsWidget->TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PlayerController->SetInputMode(InputMode);
			
			UE_LOG(LogTemp, Warning, TEXT("UIManager: Skills panel opened - cursor enabled and focus set"));
			UE_LOG(LogTemp, Warning, TEXT("UIManager: PlayerController state - ShowCursor: %s, ClickEvents: %s, MouseOver: %s"), 
				PlayerController->bShowMouseCursor ? TEXT("true") : TEXT("false"),
				PlayerController->bEnableClickEvents ? TEXT("true") : TEXT("false"),
				PlayerController->bEnableMouseOverEvents ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: PlayerController not available for cursor setup"));
		}
	}
	else
	{
		AvailableSkillsWidget->SetVisibility(ESlateVisibility::Hidden);
		
		// Restore game input mode when closing
		if (PlayerController)
		{
			// Keep cursor for other UI elements, but set focus back to game
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PlayerController->SetInputMode(InputMode);
			
			UE_LOG(LogTemp, Warning, TEXT("UIManager: Skills panel closed - focus restored to game"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UIManager: Skills panel %s"), bSkillsPanelVisible ? TEXT("opened") : TEXT("closed"));
}

void UUIManager::HandleSkillsPanelToggle(const FInputActionValue& Value)
{
	if (Value.Get<bool>())
	{
		ToggleSkillsPanel();
	}
}

void UUIManager::SetSkillTarget(int32 TargetId, ECasterType TargetType)
{
	if (SkillBarWidget)
	{
		SkillBarWidget->SetCurrentTarget(TargetId, TargetType);
		//UE_LOG(LogTemp, Log, TEXT("UIManager: Set skill target to %d (%s)"), TargetId, *UEnum::GetValueAsString(TargetType));
	}
}

void UUIManager::SetPlayerController(APlayerController* InPlayerController)
{
	PlayerController = InPlayerController;
	UE_LOG(LogTemp, Warning, TEXT("UIManager: PlayerController reference set successfully"));
}