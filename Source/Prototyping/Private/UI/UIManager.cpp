#include "UI/UIManager.h"
#include "UI/InventoryWidget.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UUIManager::UUIManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	InventoryWidget = nullptr;
	HarvestProgressWidget = nullptr;
	HarvestLootWidget = nullptr;
	InventoryManager = nullptr;
	HarvestManager = nullptr;
	FCTManager = nullptr;
	PlayerController = nullptr;
	RootCanvas = nullptr;
	bIsInitialized = false;
}

void UUIManager::BeginPlay()
{
	Super::BeginPlay();
	
	// UI creation is handled manually through Initialize() or CreateUIWidgets()
	// This allows for better control over when UI is created
}

void UUIManager::Initialize(UInventoryManager* InInventoryManager, UHarvestManager* InHarvestManager)
{
	if (!InInventoryManager)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Cannot initialize with null InventoryManager"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("UIManager: Starting initialization"));

	InventoryManager = InInventoryManager;
	HarvestManager = InHarvestManager;
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Managers set - InventoryManager: %s, HarvestManager: %s"), 
		InventoryManager ? TEXT("Valid") : TEXT("NULL"), 
		HarvestManager ? TEXT("Valid") : TEXT("NULL"));
	
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

	// Add to viewport
	InventoryWidget->AddToViewport();

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