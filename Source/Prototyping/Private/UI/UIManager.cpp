#include "UI/UIManager.h"
#include "UI/InventoryWidget.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "UI/AvailableSkillsWidget.h"
#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/DamageCanvasWidget.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MyGameInstance.h"

UUIManager::UUIManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	PlayerInterfaceWidget = nullptr;
	InventoryWidget = nullptr;
	HarvestProgressWidget = nullptr;
	HarvestLootWidget = nullptr;
	AvailableSkillsWidget = nullptr;
	InventoryManager = nullptr;
	HarvestManager = nullptr;
	ExperienceManager = nullptr;
	SkillManager = nullptr;
	FCTManager = nullptr;
	PlayerController = nullptr;
	GameVersionWidget = nullptr;
	bIsInitialized = false;
	
	// Initialize widget visibility tracking
	bInventoryVisible = false;
	bSkillsPanelVisible = false;
	bHarvestLootVisible = false;
}

void UUIManager::BeginPlay()
{
	Super::BeginPlay();
	
	// UI creation is handled manually through Initialize() or CreateUIWidgets()
	// This allows for better control over when UI is created
}

void UUIManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unsubscribe from widget events before cleanup
	if (AvailableSkillsWidget)
	{
		AvailableSkillsWidget->OnWidgetVisibilityChanged.RemoveDynamic(this, &UUIManager::OnAvailableSkillsVisibilityChanged);
	}
	
	if (InventoryWidget)
	{
		InventoryWidget->OnInventoryVisibilityChanged.RemoveDynamic(this, &UUIManager::OnInventoryVisibilityChanged);
	}
	
	if (HarvestLootWidget)
	{
		HarvestLootWidget->OnHarvestLootVisibilityChanged.RemoveDynamic(this, &UUIManager::OnHarvestLootVisibilityChanged);
	}
	
	Super::EndPlay(EndPlayReason);
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

void UUIManager::InitFTCManager(APlayerController* InPC)
{
	PlayerController = InPC;
	UDamageCanvasWidget* InDamageCanvasWidget = GetDamageCanvasWidget();
	TSubclassOf<UDamageTextWidget> InDamageTextClass = DamageTextWidgetClass;

	UE_LOG(LogTemp, Warning, TEXT("UIManager::InitFTCManager called with PC: %s, DamageCanvasWidget: %s, DamageTextClass: %s"),
		InPC ? TEXT("Valid") : TEXT("NULL"),
		InDamageCanvasWidget ? TEXT("Valid") : TEXT("NULL"),
		InDamageTextClass ? TEXT("Valid") : TEXT("NULL"));

	// Validate all required parameters before proceeding
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager::InitFTCManager - PlayerController is null"));
		return;
	}

	if (!InDamageCanvasWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager::InitFTCManager - DamageCanvasWidget is null"));
		return;
	}

	if (!InDamageCanvasWidget->GetDamageCanvas())
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager::InitFTCManager - DamageCanvas is null"));
		return;
	}

	if (!InDamageTextClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager::InitFTCManager - DamageTextClass is null"));
		return;
	}

	// Create FCT Manager if it doesn't exist
	if (!FCTManager)
	{
		FCTManager = NewObject<UFloatingCombatTextManager>(this);
		if (!FCTManager)
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager::InitFTCManager - Failed to create FCTManager object"));
			return;
		}
	}

	// Initialize the FCT Manager
	FCTManager->Init(InDamageCanvasWidget->GetDamageCanvas(), PlayerController, InDamageTextClass);

	UE_LOG(LogTemp, Warning, TEXT("UIManager: FCT Manager initialized successfully with DamageCanvasWidget"));

	// Verify the FCTManager is properly set up
	if (FCTManager->GetRootCanvas() && FCTManager->GetPlayerController() && FCTManager->GetDamageTextClass())
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: FCT Manager validation successful - all components ready"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: FCT Manager validation failed after init"));
		// Don't null out FCTManager here, as it might still be partially functional
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

	// Create player interface widget first (now includes experience widget)
	CreatePlayerInterfaceWidget();
	
	// Create inventory widget
	CreateInventoryWidget();
	
	// Create harvest widgets
	CreateHarvestWidgets();

	// Create skill widgets (now handled by PlayerInterfaceWidget)
	CreateSkillWidgets();

	// Create game version widget
	CreateGameVersionWidget();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: All UI widgets created successfully"));
}

void UUIManager::CreatePlayerInterfaceWidget()
{
	if (!PlayerInterfaceWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: PlayerInterfaceWidgetClass is not set"));
		return;
	}

	if (PlayerInterfaceWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Player interface widget already exists, removing old one"));
		PlayerInterfaceWidget->RemoveFromParent();
		PlayerInterfaceWidget = nullptr;
	}

	// Create the widget
	PlayerInterfaceWidget = CreateWidget<UPlayerInterfaceWidget>(GetWorld(), PlayerInterfaceWidgetClass);
	if (!PlayerInterfaceWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create player interface widget"));
		return;
	}

	// Add to viewport with appropriate Z-Order
	PlayerInterfaceWidget->AddToViewport(10);

	// Initialize with GameInstance
	if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
	{
		PlayerInterfaceWidget->InterfaceInitialize(GameInstance);
		
		// Setup skill bar
		PlayerInterfaceWidget->SetupSkillBar(SkillBarSlots);
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Player interface widget created and initialized"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to get GameInstance for player interface widget"));
	}
}

USkillBarWidget* UUIManager::GetSkillBarWidget() const
{
	if (PlayerInterfaceWidget)
	{
		return PlayerInterfaceWidget->GetSkillBarWidget();
	}
	return nullptr;
}

UDamageCanvasWidget* UUIManager::GetDamageCanvasWidget() const
{
	if (PlayerInterfaceWidget)
	{
		return PlayerInterfaceWidget->GetDamageCanvasWidget();
	}
	return nullptr;
}

UPlayerExperienceWidget* UUIManager::GetPlayerExperienceWidget() const
{
	if (PlayerInterfaceWidget)
	{
		return PlayerInterfaceWidget->GetPlayerExperienceWidget();
	}
	return nullptr;
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
		InventoryWidget->OnInventoryVisibilityChanged.RemoveDynamic(this, &UUIManager::OnInventoryVisibilityChanged);
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

	// Bind to inventory visibility events
	InventoryWidget->OnInventoryVisibilityChanged.AddDynamic(this, &UUIManager::OnInventoryVisibilityChanged);

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
			HarvestLootWidget->OnHarvestLootVisibilityChanged.RemoveDynamic(this, &UUIManager::OnHarvestLootVisibilityChanged);
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
			
			// Bind to harvest loot visibility events
			HarvestLootWidget->OnHarvestLootVisibilityChanged.AddDynamic(this, &UUIManager::OnHarvestLootVisibilityChanged);
			
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
		// Обновляем состояние видимости инвентаря
		bool bWasVisible = bInventoryVisible;
		InventoryManager->ToggleInventoryUI();
		
		// Определяем новое состояние (инвертируем предыдущее)
		bInventoryVisible = !bWasVisible;
		
		// Обновляем курсор и режим ввода
		UpdateCursorAndInputMode();
		
		UE_LOG(LogTemp, Log, TEXT("UIManager: Toggled inventory UI - now %s"), 
			bInventoryVisible ? TEXT("visible") : TEXT("hidden"));
	}
}

void UUIManager::HandleInventoryToggle(const FInputActionValue& Value)
{
	if (Value.Get<bool>())
	{
		ToggleInventory();
	}
}

void UUIManager::InitializeExperienceWidget(int32 CharacterId)
{
	if (!PlayerInterfaceWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: PlayerInterfaceWidget not created yet"));
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

	// Delegate initialization to PlayerInterfaceWidget
	PlayerInterfaceWidget->InitializeExperienceWidget(ExperienceManager, CharacterId);
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Experience widget initialization delegated to PlayerInterfaceWidget for character %d"), CharacterId);
}

void UUIManager::CreateSkillWidgets()
{
	// Skills are now handled by PlayerInterfaceWidget
	// This method remains for AvailableSkillsWidget only
	
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
			
			// Subscribe to visibility change events
			AvailableSkillsWidget->OnWidgetVisibilityChanged.AddDynamic(this, &UUIManager::OnAvailableSkillsVisibilityChanged);
			
			// Get game instance for initialization
			if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
			{
				AvailableSkillsWidget->SkillInitialize(GameInstance);
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

	// Skill bar is now handled by PlayerInterfaceWidget
	USkillBarWidget* SkillBarWidget = GetSkillBarWidget();
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

	// Используем текущее состояние видимости виджета вместо нашей переменной
	bool bCurrentlyVisible = AvailableSkillsWidget->IsWidgetVisible();
	
	if (!bCurrentlyVisible)
	{
		// ИСПРАВЛЕНО: Используем ShowWidget() вместо SetVisibility(SelfHitTestInvisible)
		AvailableSkillsWidget->ShowWidget();
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Skills panel opened using ShowWidget()"));
	}
	else
	{
		// Используем HideWidget() для последовательности
		AvailableSkillsWidget->HideWidget();
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Skills panel closed using HideWidget()"));
	}

	// bSkillsPanelVisible будет автоматически обновлена через OnAvailableSkillsVisibilityChanged
	UE_LOG(LogTemp, Log, TEXT("UIManager: Skills panel %s"), !bCurrentlyVisible ? TEXT("opened") : TEXT("closed"));
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
	USkillBarWidget* SkillBarWidget = GetSkillBarWidget();
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

void UUIManager::OnAvailableSkillsVisibilityChanged(bool bIsVisible)
{
	// Синхронизоруем состояние видимости с внутренним состоянием виджета
	bSkillsPanelVisible = bIsVisible;
	
	// Обновляем курсор и режим ввода
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Skills panel visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnInventoryVisibilityChanged(bool bIsVisible)
{
	bInventoryVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Inventory visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnHarvestLootVisibilityChanged(bool bIsVisible)
{
	bHarvestLootVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Harvest loot visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::UpdateCursorAndInputMode()
{
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Cannot update cursor - PlayerController not set"));
		return;
	}

	bool bShouldShow = ShouldShowCursor();
	PlayerController->bShowMouseCursor = bShouldShow;

	if (bShouldShow)
	{
		// Есть активные UI виджеты - используем режим Game+UI
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Cursor shown - Game+UI mode"));
	}
	else
	{
		// Нет активных UI виджетов - переходим в игровой режим
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Cursor hidden - Game only mode"));
	}
}

bool UUIManager::ShouldShowCursor() const
{
	// Показываем курсор если хотя бы один из UI виджетов видим
	bool bAnyWidgetVisible = bInventoryVisible || bSkillsPanelVisible || bHarvestLootVisible;
	
	UE_LOG(LogTemp, Verbose, TEXT("UIManager: Cursor check - Inventory: %s, Skills: %s, Harvest: %s -> Show: %s"),
		bInventoryVisible ? TEXT("visible") : TEXT("hidden"),
		bSkillsPanelVisible ? TEXT("visible") : TEXT("hidden"),
		bHarvestLootVisible ? TEXT("visible") : TEXT("hidden"),
		bAnyWidgetVisible ? TEXT("YES") : TEXT("NO"));
	
	return bAnyWidgetVisible;
}