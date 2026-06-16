#include "UI/UIManager.h"
#include "UI/UIManager.h"
#include "UI/InventoryWidget.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "UI/AvailableSkillsWidget.h"
#include "UI/DialogueWidget.h"
#include "UI/QuestJournalWidget.h"
#include "UI/QuestTrackerWidget.h"
#include "UI/VendorShopWidget.h"
#include "UI/RepairShopWidget.h"
#include "UI/SkillShopWidget.h"
#include "UI/TradeWidget.h"
#include "UI/EquipmentWidget.h"
#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/DamageCanvasWidget.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/UI/DeathScreenWidget.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "UI/PlayerStatsWidget.h"
#include "UI/BestiaryWidget.h"
#include "UI/ChatWidget.h"
#include "UI/GameMenuBarWidget.h"
#include "UI/GameMenuWidget.h"
#include "UI/AudioSettingsWidget.h"
#include "UI/W_SettingsWidget.h"
#include "UI/NotificationToastWidget.h"
#include "UI/NotificationZoneBannerWidget.h"
#include "UI/NotificationScreenCenterWidget.h"
#include "UI/NotificationAtmosphereWidget.h"
#include "UI/WorldNotificationManager.h"
#include "UI/TitlesWidget.h"
#include "UI/ReputationWidget.h"
#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Player/TitleNetworkHandler.h"
#include "Gameplay/Player/ReputationManager.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "UI/EmoteListWidget.h"
#include "Audio/AudioManager.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Engine.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "MyGameInstance.h"
#include "Gameplay/Combat/CombatCameraShake.h"
#include "Gameplay/UI/CombatScreenFlashWidget.h"
#include "Gameplay/UI/MobTargetFrameWidget.h"
#include "Gameplay/UI/NameplateManager.h"
#include "Gameplay/UI/NameplateCanvasWidget.h"
#include "UI/WIOInteractionPromptWidget.h"
#include "UI/WIOChannelBarWidget.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Services/LocalizationSubsystem.h"

UUIManager::UUIManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	
	PlayerInterfaceWidget = nullptr;
	InventoryWidget = nullptr;
	HarvestProgressWidget = nullptr;
	HarvestLootWidget = nullptr;
	AvailableSkillsWidget = nullptr;
	DialogueWidget = nullptr;
	QuestJournalWidget = nullptr;
	QuestTrackerWidget = nullptr;
	VendorShopWidget = nullptr;
	RepairShopWidget = nullptr;
	SkillShopWidget = nullptr;
	TradeWidget = nullptr;
	EquipmentWidget = nullptr;
	PlayerStatsWidget = nullptr;
	BestiaryWidget = nullptr;
	ChatWidget = nullptr;
	GameMenuBarWidget = nullptr;
	GameMenuWidget = nullptr;
	AudioSettingsWidget = nullptr;
	GameSettingsWidget  = nullptr;
	TitlesWidget = nullptr;
	ReputationWidget = nullptr;
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
	bDialogueVisible = false;
	bQuestJournalVisible = false;
	bVendorShopVisible = false;
	bRepairShopVisible = false;
	bSkillShopVisible = false;
	bTradeVisible = false;
	bEquipmentVisible = false;
	bPlayerStatsVisible = false;
	bBestiaryVisible = false;
	// bAltCursorActive = true keeps the cursor visible in world interaction mode at all times.
	// UpdateCursorAndInputMode() uses ShouldShowCursor() which includes this flag.
	bAltCursorActive = true;
	bGameMenuVisible = false;
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

	if (DialogueWidget)
	{
		DialogueWidget->OnDialogueVisibilityChanged.RemoveDynamic(this, &UUIManager::OnDialogueVisibilityChanged);
	}

	if (QuestJournalWidget)
	{
		QuestJournalWidget->OnQuestJournalVisibilityChanged.RemoveDynamic(this, &UUIManager::OnQuestJournalVisibilityChanged);
	}

	if (VendorShopWidget)
	{
		VendorShopWidget->OnVendorShopVisibilityChanged.RemoveDynamic(this, &UUIManager::OnVendorShopVisibilityChanged);
	}

	if (RepairShopWidget)
	{
		RepairShopWidget->OnRepairShopVisibilityChanged.RemoveDynamic(this, &UUIManager::OnRepairShopVisibilityChanged);
	}

	if (TradeWidget)
	{
		TradeWidget->OnTradeVisibilityChanged.RemoveDynamic(this, &UUIManager::OnTradeVisibilityChanged);
	}

	if (EquipmentWidget)
	{
		EquipmentWidget->OnEquipmentVisibilityChanged.RemoveDynamic(this, &UUIManager::OnEquipmentVisibilityChanged);
	}

	if (PlayerStatsWidget)
	{
		PlayerStatsWidget->OnStatsVisibilityChanged.RemoveDynamic(this, &UUIManager::OnPlayerStatsVisibilityChanged);
	}

	if (BestiaryWidget)
	{
		BestiaryWidget->OnBestiaryVisibilityChanged.RemoveDynamic(this, &UUIManager::OnBestiaryVisibilityChanged);
	}

	if (DeathScreenWidget)
	{
		if (ABasicPlayer* OwnerPlayer = Cast<ABasicPlayer>(GetOwner()))
		{
			DeathScreenWidget->OnRespawnRequested.RemoveDynamic(OwnerPlayer, &ABasicPlayer::OnRespawnClicked);
		}
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

	// If death screen was requested before PlayerController was ready, show it now
	if (bPendingDeathScreen)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Showing pending death screen (debt=%d)"), PendingDeathScreenDebt);
		ShowDeathScreen(PendingDeathScreenDebt);
		bPendingDeathScreen = false;
		PendingDeathScreenDebt = 0;
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
	// Create game menu bar (always-visible bottom bar)
	CreateGameMenuBarWidget();

	// Create game menu (opened on Escape)
	CreateGameMenuWidget();
	
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: All UI widgets created successfully"));
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

	// Subscribe to the widget's own ready signal.
	// PlayerInterfaceWidget::NativeTick fires OnPlayerInterfaceReady on the first
	// game-thread tick where all child widgets are valid, which is guaranteed to
	// be at least one full render frame after AddToViewport.
	// This replaces the old SetTimer(0.0f) approach which never fired in UE5.
	PlayerInterfaceWidget->OnPlayerInterfaceReady.AddDynamic(this, &UUIManager::HandlePlayerInterfaceReady);

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

void UUIManager::CreateGameMenuBarWidget()
{
	if (!GameMenuBarWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: GameMenuBarWidgetClass is not set in Blueprint"));
		return;
	}

	if (GameMenuBarWidget)
	{
		GameMenuBarWidget->RemoveFromParent();
		GameMenuBarWidget = nullptr;
	}

	GameMenuBarWidget = CreateWidget<UGameMenuBarWidget>(GetWorld(), GameMenuBarWidgetClass);
	if (!GameMenuBarWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create GameMenuBarWidget"));
		return;
	}

	GameMenuBarWidget->AddToViewport(20);

	// Wire all buttons to UIManager actions
	GameMenuBarWidget->OnInventoryClicked.AddDynamic(this,    &UUIManager::OnMenuBarInventoryClicked);
	GameMenuBarWidget->OnEquipmentClicked.AddDynamic(this,    &UUIManager::OnMenuBarEquipmentClicked);
	GameMenuBarWidget->OnQuestJournalClicked.AddDynamic(this, &UUIManager::OnMenuBarQuestJournalClicked);
	GameMenuBarWidget->OnSkillsClicked.AddDynamic(this,       &UUIManager::OnMenuBarSkillsClicked);
	GameMenuBarWidget->OnStatsClicked.AddDynamic(this,        &UUIManager::OnMenuBarStatsClicked);
	GameMenuBarWidget->OnBestiaryClicked.AddDynamic(this,     &UUIManager::OnMenuBarBestiaryClicked);
	GameMenuBarWidget->OnTitlesClicked.AddDynamic(this,       &UUIManager::OnMenuBarTitlesClicked);
	GameMenuBarWidget->OnReputationClicked.AddDynamic(this,   &UUIManager::OnMenuBarReputationClicked);
	GameMenuBarWidget->OnEmoteClicked.AddDynamic(this,        &UUIManager::OnMenuBarEmoteClicked);
	GameMenuBarWidget->OnMenuClicked.AddDynamic(this,         &UUIManager::OnMenuBarMenuClicked);

	UE_LOG(LogTemp, Log, TEXT("UIManager: GameMenuBarWidget created and wired"));
}

void UUIManager::CreateGameMenuWidget()
{
	if (!GameMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: GameMenuWidgetClass is not set in Blueprint"));
		return;
	}

	if (GameMenuWidget)
	{
		GameMenuWidget->RemoveFromParent();
		GameMenuWidget = nullptr;
	}

	GameMenuWidget = CreateWidget<UGameMenuWidget>(GetWorld(), GameMenuWidgetClass);
	if (!GameMenuWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create GameMenuWidget"));
		return;
	}

	GameMenuWidget->AddToViewport(200);

	// Wire game menu delegates to UIManager handlers
	GameMenuWidget->OnResumeClicked.AddDynamic(this,    &UUIManager::HandleGameMenuResumeClicked);
	GameMenuWidget->OnSettingsClicked.AddDynamic(this,  &UUIManager::HandleSettingsClicked);
	GameMenuWidget->OnExitToLoginClicked.AddDynamic(this,   &UUIManager::HandleExitToLoginClicked);
	GameMenuWidget->OnExitToDesktopClicked.AddDynamic(this, &UUIManager::HandleExitToDesktopClicked);

	UE_LOG(LogTemp, Warning, TEXT("UIManager: GameMenuWidget created and wired (class: %s)"),
		*GameMenuWidgetClass->GetName());
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
		// ��������� ��������� ��������� ���������
		bool bWasVisible = bInventoryVisible;
		InventoryManager->ToggleInventoryUI();
		
		// ���������� ����� ��������� (����������� ����������)
		bInventoryVisible = !bWasVisible;
		
		// ��������� ������ � ����� �����
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
			// Add to viewport with same Z-order as other draggable windows
			AvailableSkillsWidget->AddToViewport(90);
			
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

	// ���������� ������� ��������� ��������� ������� ������ ����� ����������
	bool bCurrentlyVisible = AvailableSkillsWidget->IsWidgetVisible();
	
	if (!bCurrentlyVisible)
	{
		// ����������: ���������� ShowWidget() ������ SetVisibility(SelfHitTestInvisible)
		AvailableSkillsWidget->ShowWidget();
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Skills panel opened using ShowWidget()"));
	}
	else
	{
		// ���������� HideWidget() ��� ������������������
		AvailableSkillsWidget->HideWidget();
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Skills panel closed using HideWidget()"));
	}

	// bSkillsPanelVisible ����� ������������� ��������� ����� OnAvailableSkillsVisibilityChanged
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
	// �������������� ��������� ��������� � ���������� ���������� �������
	bSkillsPanelVisible = bIsVisible;
	
	// ��������� ������ � ����� �����
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Skills panel visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::HandlePlayerInterfaceReady()
{
	if (PlayerInterfaceWidget)
	{
		PlayerInterfaceWidget->OnPlayerInterfaceReady.RemoveDynamic(this, &UUIManager::HandlePlayerInterfaceReady);
	}

	// Create NameplateManager and bind to the canvas widget from PlayerInterfaceWidget
	if (!NameplateManager && PlayerInterfaceWidget)
	{
		NameplateManager = NewObject<UNameplateManager>(this);
		if (NameplateManager)
		{
			NameplateManager->RegisterComponent();
			UNameplateCanvasWidget* Canvas = PlayerInterfaceWidget->GetNameplateCanvasWidget();
			if (Canvas)
			{
				NameplateManager->SetCanvasWidget(Canvas);
				UE_LOG(LogTemp, Warning, TEXT("UIManager: NameplateManager created and bound to canvas widget"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UIManager: NameplateManager created but NameplateCanvasWidget not found in PlayerInterfaceWidget. Add it to the WBP_PlayerInterface Blueprint."));
			}
		}
	}

	OnUIManagerInitialized.Broadcast();

	// Initialize item quickbar if the widget is present in the PlayerInterface
	if (PlayerInterfaceWidget && InventoryManager)
	{
		if (UItemQuickBarWidget* QuickBar = PlayerInterfaceWidget->GetItemQuickBar())
		{
			QuickBar->InitQuickBar(InventoryManager);
			InventoryManager->OnInventoryUpdated.AddDynamic(QuickBar, &UItemQuickBarWidget::HandleInventoryUpdated);
			UE_LOG(LogTemp, Warning, TEXT("UIManager: ItemQuickBar initialized"));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] HandlePlayerInterfaceReady: OnUIManagerInitialized broadcast"));
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

void UUIManager::OnDialogueVisibilityChanged(bool bIsVisible)
{
	bDialogueVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Dialogue visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnQuestJournalVisibilityChanged(bool bIsVisible)
{
	bQuestJournalVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Quest journal visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnVendorShopVisibilityChanged(bool bIsVisible)
{
	bVendorShopVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Vendor shop visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnRepairShopVisibilityChanged(bool bIsVisible)
{
	bRepairShopVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Repair shop visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnSkillShopVisibilityChanged(bool bIsVisible)
{
	bSkillShopVisible = bIsVisible;
	UpdateCursorAndInputMode();

	UE_LOG(LogTemp, Warning, TEXT("UIManager: Skill shop visibility synced: %s"),
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnTradeVisibilityChanged(bool bIsVisible)
{
	bTradeVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Trade visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnEquipmentVisibilityChanged(bool bIsVisible)
{
	bEquipmentVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Equipment visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnPlayerStatsVisibilityChanged()
{
	bPlayerStatsVisible = PlayerStatsWidget && PlayerStatsWidget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Player stats visibility synced: %s"), 
		bPlayerStatsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnTitlesVisibilityChanged()
{
	bTitlesVisible = TitlesWidget && TitlesWidget->GetVisibility() == ESlateVisibility::Visible;
	UpdateCursorAndInputMode();
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Titles visibility synced: %s"), bTitlesVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnReputationVisibilityChanged()
{
	bReputationVisible = ReputationWidget && ReputationWidget->GetVisibility() == ESlateVisibility::Visible;
	UpdateCursorAndInputMode();
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Reputation visibility synced: %s"), bReputationVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnBestiaryVisibilityChanged(bool bIsVisible)
{
	bBestiaryVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogTemp, Warning, TEXT("UIManager: Bestiary visibility synced: %s"), 
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
		// ���� �������� UI ������� - ���������� ����� Game+UI
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Cursor shown - Game+UI mode"));
	}
	else
	{
		// ��� �������� UI �������� - ��������� � ������� �����
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
		
		UE_LOG(LogTemp, Warning, TEXT("UIManager: Cursor hidden - Game only mode"));
	}
}

bool UUIManager::HasUIWindowOpen() const
{
	// True only when an actual panel window is open and consuming cursor input.
	// Intentionally excludes bAltCursorActive so world interaction is never blocked
	// just because the cursor is always shown.
    return bInventoryVisible || bSkillsPanelVisible || bHarvestLootVisible
        || bDialogueVisible || bQuestJournalVisible
        || bVendorShopVisible || bRepairShopVisible || bSkillShopVisible || bTradeVisible || bEquipmentVisible
        || bPlayerStatsVisible || bBestiaryVisible || bTitlesVisible || bReputationVisible || bEmoteListVisible;
}

bool UUIManager::HasModalWindowOpen() const
{
	// Only windows that should fully block world interaction.
	// Non-modal windows (inventory, stats, skills, etc.) let UMG hit-testing
	// handle click-through: clicks on UI elements are consumed, clicks on
	// transparent/empty areas pass through to the game world.
    return bDialogueVisible || bTradeVisible || bHarvestLootVisible;
}

bool UUIManager::HasCursorOverWindowContent() const
{
	if (!bInventoryVisible    && !bSkillsPanelVisible && !bHarvestLootVisible &&
		!bDialogueVisible     && !bQuestJournalVisible && !bVendorShopVisible  &&
		!bRepairShopVisible   && !bSkillShopVisible    && !bTradeVisible       &&
		!bEquipmentVisible    && !bPlayerStatsVisible  && !bBestiaryVisible    &&
		!bTitlesVisible       && !bReputationVisible   && !bEmoteListVisible)
		return false;

	if (!FSlateApplication::IsInitialized()) return false;

	const FVector2f MousePos = FSlateApplication::Get().GetCursorPos();

	if (bInventoryVisible    && DoesWidgetTreeHaveHoveredChild(InventoryWidget,    MousePos)) return true;
	if (bSkillsPanelVisible  && DoesWidgetTreeHaveHoveredChild(AvailableSkillsWidget, MousePos)) return true;
	if (bHarvestLootVisible  && DoesWidgetTreeHaveHoveredChild(HarvestLootWidget,  MousePos)) return true;
	if (bDialogueVisible     && DoesWidgetTreeHaveHoveredChild(DialogueWidget,     MousePos)) return true;
	if (bQuestJournalVisible && DoesWidgetTreeHaveHoveredChild(QuestJournalWidget, MousePos)) return true;
	if (bVendorShopVisible   && DoesWidgetTreeHaveHoveredChild(VendorShopWidget,   MousePos)) return true;
	if (bRepairShopVisible   && DoesWidgetTreeHaveHoveredChild(RepairShopWidget,   MousePos)) return true;
	if (bSkillShopVisible    && DoesWidgetTreeHaveHoveredChild(SkillShopWidget,    MousePos)) return true;
	if (bTradeVisible        && DoesWidgetTreeHaveHoveredChild(TradeWidget,        MousePos)) return true;
	if (bEquipmentVisible    && DoesWidgetTreeHaveHoveredChild(EquipmentWidget,    MousePos)) return true;
	if (bPlayerStatsVisible  && DoesWidgetTreeHaveHoveredChild(PlayerStatsWidget,  MousePos)) return true;
	if (bBestiaryVisible     && DoesWidgetTreeHaveHoveredChild(BestiaryWidget,     MousePos)) return true;
	if (bTitlesVisible       && DoesWidgetTreeHaveHoveredChild(TitlesWidget,       MousePos)) return true;
	if (bReputationVisible   && DoesWidgetTreeHaveHoveredChild(ReputationWidget,   MousePos)) return true;
	if (bEmoteListVisible    && DoesWidgetTreeHaveHoveredChild(EmoteListWidget,    MousePos)) return true;
	return false;
}

bool UUIManager::DoesWidgetTreeHaveHoveredChild(UWidget* Widget, const FVector2f& MousePos)
{
	if (!Widget || !Widget->IsVisible()) return false;

	// Only EVisibility::Visible widgets can block — SelfHitTestInvisible pass through
	if (Widget->GetVisibility() == ESlateVisibility::Visible &&
		Widget->GetCachedGeometry().IsUnderLocation(FVector2D(MousePos)))
		return true;

	// Traverse into UUserWidget's widget tree (InventoryWidget, DialogueWidget, etc.)
	if (UUserWidget* UserW = Cast<UUserWidget>(Widget))
	{
		if (UserW->WidgetTree && UserW->WidgetTree->RootWidget)
		{
			if (DoesWidgetTreeHaveHoveredChild(UserW->WidgetTree->RootWidget, MousePos))
				return true;
		}
	}

	// Traverse panel children (CanvasPanel, VerticalBox, etc.)
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			if (DoesWidgetTreeHaveHoveredChild(Panel->GetChildAt(i), MousePos))
				return true;
		}
	}
	return false;
}

int32 UUIManager::GetActiveInteractionNpcId() const
{
	if (bDialogueVisible && DialogueWidget)
	{
		const int32 Id = DialogueWidget->GetCurrentSpeakerNpcId();
		if (Id > 0) return Id;
	}
	if (bVendorShopVisible && VendorShopWidget)
	{
		const int32 Id = VendorShopWidget->GetActiveNpcId();
		if (Id > 0) return Id;
	}
	if (bRepairShopVisible && RepairShopWidget)
	{
		const int32 Id = RepairShopWidget->GetActiveNpcId();
		if (Id > 0) return Id;
	}
	if (bSkillShopVisible && SkillShopWidget)
	{
		const int32 Id = SkillShopWidget->GetActiveNpcId();
		if (Id > 0) return Id;
	}
	return 0;
}

void UUIManager::ForceCloseAllNPCWindows(UDialogueManager* DlgMgr)
{
	// Close dialogue: send server packet then hide widget immediately.
	if (bDialogueVisible && DialogueWidget)
	{
		if (DlgMgr && DlgMgr->IsDialogueActive())
			DlgMgr->CloseDialogue();
		DialogueWidget->HideDialogue();
	}
	// Close shops (each CloseShop also handles the NPC farewell-window counter).
	if (bVendorShopVisible && VendorShopWidget)
		VendorShopWidget->CloseShop();
	if (bRepairShopVisible && RepairShopWidget)
		RepairShopWidget->CloseShop();
	if (bSkillShopVisible && SkillShopWidget)
		SkillShopWidget->CloseShop();
}

bool UUIManager::ShouldShowCursor() const
{
	// Show cursor if any UI element is open or alt-cursor is active
	bool bAnyWidgetVisible = bInventoryVisible || bSkillsPanelVisible || bHarvestLootVisible 
		|| bDialogueVisible || bQuestJournalVisible
		|| bVendorShopVisible || bRepairShopVisible || bSkillShopVisible || bTradeVisible || bEquipmentVisible
		|| bPlayerStatsVisible || bBestiaryVisible || bTitlesVisible || bReputationVisible || bEmoteListVisible
		|| bAltCursorActive || bGameMenuVisible;
	
	UE_LOG(LogTemp, Verbose, TEXT("UIManager: Cursor check -> Show: %s"),
		bAnyWidgetVisible ? TEXT("YES") : TEXT("NO"));
	
	return bAnyWidgetVisible;
}

void UUIManager::PlayCombatCameraShake(float Intensity)
{
	if (!GetWorld()) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;

	// Prefer the Blueprint-assigned class; fall back to the C++ default.
	TSubclassOf<UCameraShakeBase> ShakeClass = CombatCameraShakeClass
		? CombatCameraShakeClass
		: TSubclassOf<UCameraShakeBase>(UCombatCameraShake::StaticClass());

	PC->PlayerCameraManager->StartCameraShake(ShakeClass, Intensity);
}

void UUIManager::ShowMobTargetFrame(const FString& MobSlug, const FString& MobName, int32 MobLevel, int32 CurrentHP, int32 MaxHP, bool bIsAggro, UTexture2D* MobIcon)
{
	if (PlayerInterfaceWidget)
	{
		UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget();
		if (TargetFrame)
		{
			TargetFrame->SetMobInfo(MobSlug, MobName, MobLevel, CurrentHP, MaxHP, bIsAggro, MobIcon);
		}
	}
}

void UUIManager::HideMobTargetFrame()
{
	if (PlayerInterfaceWidget)
	{
		UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget();
		if (TargetFrame)
		{
			TargetFrame->ClearTarget();
		}
	}
}

void UUIManager::ToggleGameMenu()
{
	UE_LOG(LogTemp, Warning, TEXT("UIManager::ToggleGameMenu - GameMenuWidget: %s, IsMenuOpen: %s"),
		GameMenuWidget ? TEXT("Valid") : TEXT("NULL"),
		(GameMenuWidget && GameMenuWidget->IsMenuOpen()) ? TEXT("true") : TEXT("false"));

	// Toggle the game menu only — do not close other windows.
	if (GameMenuWidget)
	{
		if (GameMenuWidget->IsMenuOpen())
		{
			GameMenuWidget->CloseMenu();
			bGameMenuVisible = false;
		}
		else
		{
			GameMenuWidget->OpenMenu();
			bGameMenuVisible = true;
		}
		UpdateCursorAndInputMode();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager::ToggleGameMenu - GameMenuWidget is NULL!"));
	}
}

void UUIManager::InitializeDialogueAndQuestWidgets(UDialogueManager* InDialogueManager, UQuestManager* InQuestManager)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeDialogueAndQuestWidgets - Starting initialization"));

	if (!InDialogueManager || !InQuestManager)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager::InitializeDialogueAndQuestWidgets - DialogueManager or QuestManager is null!"));
		return;
	}

	// Create dialogue widget if class is set
	if (DialogueWidgetClass && !DialogueWidget)
	{
		DialogueWidget = CreateWidget<UDialogueWidget>(GetWorld(), DialogueWidgetClass);
		if (DialogueWidget)
		{
			DialogueWidget->AddToViewport(100); // High Z-order for dialogue
			DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
			DialogueWidget->BindToDialogueManager(InDialogueManager);
			DialogueWidget->OnDialogueVisibilityChanged.AddDynamic(this, &UUIManager::OnDialogueVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: DialogueWidget created and bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create DialogueWidget"));
		}
	}
	else if (!DialogueWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: DialogueWidgetClass is not set in Blueprint"));
	}

	// Create quest journal widget if class is set
	if (QuestJournalWidgetClass && !QuestJournalWidget)
	{
		QuestJournalWidget = CreateWidget<UQuestJournalWidget>(GetWorld(), QuestJournalWidgetClass);
		if (QuestJournalWidget)
		{
			QuestJournalWidget->AddToViewport(100); // High Z-order for journal
			QuestJournalWidget->SetVisibility(ESlateVisibility::Collapsed);
			QuestJournalWidget->BindToQuestManager(InQuestManager);
			QuestJournalWidget->OnQuestJournalVisibilityChanged.AddDynamic(this, &UUIManager::OnQuestJournalVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: QuestJournalWidget created and bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create QuestJournalWidget"));
		}
	}
	else if (!QuestJournalWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: QuestJournalWidgetClass is not set in Blueprint"));
	}

	// Create quest tracker widget if class is set (HUD overlay)
	if (QuestTrackerWidgetClass && !QuestTrackerWidget)
	{
		QuestTrackerWidget = CreateWidget<UQuestTrackerWidget>(GetWorld(), QuestTrackerWidgetClass);
		if (QuestTrackerWidget)
		{
			QuestTrackerWidget->AddToViewport(50); // Medium Z-order for HUD element
			QuestTrackerWidget->BindToQuestManager(InQuestManager);
			UE_LOG(LogTemp, Log, TEXT("UIManager: QuestTrackerWidget created and bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create QuestTrackerWidget"));
		}
	}
	else if (!QuestTrackerWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: QuestTrackerWidgetClass is not set in Blueprint"));
	}

	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeDialogueAndQuestWidgets - Completed"));
}

void UUIManager::InitializeItemSystemWidgets(UEquipmentManager* InEquipmentManager, UVendorManager* InVendorManager, URepairManager* InRepairManager, UTradeManager* InTradeManager)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeItemSystemWidgets - Starting initialization"));

	// Create equipment widget
	if (EquipmentWidgetClass && !EquipmentWidget)
	{
		EquipmentWidget = CreateWidget<UEquipmentWidget>(GetWorld(), EquipmentWidgetClass);
		if (EquipmentWidget)
		{
			EquipmentWidget->AddToViewport(90);
			EquipmentWidget->SetVisibility(ESlateVisibility::Collapsed);
			if (InEquipmentManager)
			{
				EquipmentWidget->BindToEquipmentManager(InEquipmentManager, InventoryManager);
			}
			EquipmentWidget->OnEquipmentVisibilityChanged.AddDynamic(this, &UUIManager::OnEquipmentVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: EquipmentWidget created and bound"));
		}
	}
	else if (!EquipmentWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: EquipmentWidgetClass is not set in Blueprint"));
	}

	// Create vendor shop widget
	if (VendorShopWidgetClass && !VendorShopWidget)
	{
		VendorShopWidget = CreateWidget<UVendorShopWidget>(GetWorld(), VendorShopWidgetClass);
		if (VendorShopWidget)
		{
			VendorShopWidget->AddToViewport(95);
			VendorShopWidget->SetVisibility(ESlateVisibility::Collapsed);
			if (InVendorManager)
			{
				VendorShopWidget->BindToManagers(InVendorManager, InventoryManager);
			}
			VendorShopWidget->OnVendorShopVisibilityChanged.AddDynamic(this, &UUIManager::OnVendorShopVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: VendorShopWidget created and bound"));
		}
	}
	else if (!VendorShopWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: VendorShopWidgetClass is not set in Blueprint"));
	}

	// Create repair shop widget
	if (RepairShopWidgetClass && !RepairShopWidget)
	{
		RepairShopWidget = CreateWidget<URepairShopWidget>(GetWorld(), RepairShopWidgetClass);
		if (RepairShopWidget)
		{
			RepairShopWidget->AddToViewport(95);
			RepairShopWidget->SetVisibility(ESlateVisibility::Collapsed);
			if (InRepairManager)
			{
				RepairShopWidget->BindToRepairManager(InRepairManager);
			}
			// Bind inventory so gold is populated immediately when the shop opens
			// and stays in sync after every repair.
			if (InventoryManager)
			{
				RepairShopWidget->BindToInventoryManager(InventoryManager);
			}
			RepairShopWidget->OnRepairShopVisibilityChanged.AddDynamic(this, &UUIManager::OnRepairShopVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: RepairShopWidget created and bound"));
		}
	}
	else if (!RepairShopWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: RepairShopWidgetClass is not set in Blueprint"));
	}

	// Create trade widget
	if (TradeWidgetClass && !TradeWidget)
	{
		TradeWidget = CreateWidget<UTradeWidget>(GetWorld(), TradeWidgetClass);
		if (TradeWidget)
		{
			TradeWidget->AddToViewport(95);
			TradeWidget->SetVisibility(ESlateVisibility::Collapsed);
			if (InTradeManager)
			{
				TradeWidget->BindToManagers(InTradeManager, InventoryManager);
			}
			TradeWidget->OnTradeVisibilityChanged.AddDynamic(this, &UUIManager::OnTradeVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: TradeWidget created and bound"));
		}
	}
	else if (!TradeWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: TradeWidgetClass is not set in Blueprint"));
	}

	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeItemSystemWidgets - Completed"));
}

void UUIManager::InitializeSkillShopWidget(USkillShopManager* InSkillShopManager)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeSkillShopWidget"));

	if (!SkillShopWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: SkillShopWidgetClass is not set in Blueprint"));
		return;
	}

	if (SkillShopWidget)
	{
		SkillShopWidget->OnSkillShopVisibilityChanged.RemoveDynamic(this, &UUIManager::OnSkillShopVisibilityChanged);
		SkillShopWidget->RemoveFromParent();
		SkillShopWidget = nullptr;
	}

	SkillShopWidget = CreateWidget<USkillShopWidget>(GetWorld(), SkillShopWidgetClass);
	if (!SkillShopWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create SkillShopWidget"));
		return;
	}

	SkillShopWidget->AddToViewport(95);
	SkillShopWidget->SetVisibility(ESlateVisibility::Collapsed);

	if (InSkillShopManager)
	{
		SkillShopWidget->BindToSkillShopManager(InSkillShopManager);
	}

	// Bind inventory manager so the gold display updates when getPlayerInventory arrives
	// (the server sends getPlayerInventory, not skill_learned, after a skill purchase).
	if (InventoryManager)
	{
		SkillShopWidget->BindToInventoryManager(InventoryManager);
	}

	// Bind stats manager so SP display updates when stats_update arrives
	// (e.g. from learning a passive skill or leveling up while shop is open).
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
	{
		if (UPlayerStatsManager* SM = GI->GetPlayerStatsManager())
		{
			SkillShopWidget->BindToStatsManager(SM);
		}
	}

	SkillShopWidget->OnSkillShopVisibilityChanged.AddDynamic(this, &UUIManager::OnSkillShopVisibilityChanged);
	UE_LOG(LogTemp, Log, TEXT("UIManager: SkillShopWidget created and bound"));
}

void UUIManager::InitializeStatsWidget(UPlayerStatsManager* InStatsManager)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeStatsWidget"));

	if (!InStatsManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeStatsWidget - StatsManager is null"));
		return;
	}

	if (PlayerStatsWidgetClass && !PlayerStatsWidget)
	{
		PlayerStatsWidget = CreateWidget<UPlayerStatsWidget>(GetWorld(), PlayerStatsWidgetClass);
		if (PlayerStatsWidget)
		{
			PlayerStatsWidget->AddToViewport(90);
			PlayerStatsWidget->SetVisibility(ESlateVisibility::Collapsed);
			PlayerStatsWidget->BindToStatsManager(InStatsManager);
			PlayerStatsWidget->OnStatsVisibilityChanged.AddDynamic(this, &UUIManager::OnPlayerStatsVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: PlayerStatsWidget created and bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create PlayerStatsWidget"));
		}
	}
	else if (!PlayerStatsWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: PlayerStatsWidgetClass is not set in Blueprint"));
	}

	// Also bind the HUD experience bar to the same stats manager so it updates
	// from every stats_update packet � identical to how PlayerStatsWidget works.
	if (UPlayerExperienceWidget* ExpWidget = GetPlayerExperienceWidget())
	{
		ExpWidget->BindToStatsManager(InStatsManager);
		UE_LOG(LogTemp, Log, TEXT("UIManager: PlayerExperienceWidget bound to PlayerStatsManager"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: PlayerExperienceWidget not available yet for StatsManager binding"));
	}
}


void UUIManager::InitializeTitlesWidget(UTitleManager* InTitleManager, UTitleNetworkHandler* InTitleHandler, int32 InCharacterId)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeTitlesWidget"));

	if (!InTitleManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeTitlesWidget - TitleManager is null"));
		return;
	}

	if (TitlesWidgetClass && !TitlesWidget)
	{
		TitlesWidget = CreateWidget<UTitlesWidget>(GetWorld(), TitlesWidgetClass);
		if (TitlesWidget)
		{
			TitlesWidget->AddToViewport(90);
			TitlesWidget->SetVisibility(ESlateVisibility::Collapsed);
			TitlesWidget->BindToManagers(InTitleManager, InTitleHandler, InCharacterId);
			TitlesWidget->OnTitlesVisibilityChanged.AddDynamic(this, &UUIManager::OnTitlesVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: TitlesWidget created and bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create TitlesWidget"));
		}
	}
	else if (!TitlesWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: TitlesWidgetClass is not set in Blueprint"));
	}
}

void UUIManager::InitializeReputationWidget(UReputationManager* InReputationManager)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeReputationWidget"));

	if (!InReputationManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeReputationWidget - ReputationManager is null"));
		return;
	}

	if (ReputationWidgetClass && !ReputationWidget)
	{
		ReputationWidget = CreateWidget<UReputationWidget>(GetWorld(), ReputationWidgetClass);
		if (ReputationWidget)
		{
			ReputationWidget->AddToViewport(90);
			ReputationWidget->SetVisibility(ESlateVisibility::Collapsed);
			ReputationWidget->BindToManager(InReputationManager);
			ReputationWidget->OnReputationVisibilityChanged.AddDynamic(this, &UUIManager::OnReputationVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: ReputationWidget created and bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create ReputationWidget"));
		}
	}
	else if (!ReputationWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: ReputationWidgetClass is not set in Blueprint"));
	}
}

void UUIManager::InitializeEmoteListWidget(UEmoteManager* InEmoteManager, UEmoteNetworkHandler* InEmoteHandler, int32 InCharacterId)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeEmoteListWidget"));

	if (!InEmoteManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeEmoteListWidget - EmoteManager is null"));
		return;
	}

	if (EmoteListWidgetClass && !EmoteListWidget)
	{
		EmoteListWidget = CreateWidget<UEmoteListWidget>(GetWorld(), EmoteListWidgetClass);
		if (EmoteListWidget)
		{
			EmoteListWidget->AddToViewport(90);
			EmoteListWidget->SetVisibility(ESlateVisibility::Collapsed);
			EmoteListWidget->BindToManagers(InEmoteManager, InEmoteHandler, InCharacterId);
			EmoteListWidget->OnEmoteListVisibilityChanged.AddDynamic(this, &UUIManager::OnEmoteListVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: EmoteListWidget created and bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create EmoteListWidget"));
		}
	}
	else if (!EmoteListWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: EmoteListWidgetClass is not set in Blueprint"));
	}
}

void UUIManager::OnEmoteListVisibilityChanged()
{
	bEmoteListVisible = EmoteListWidget && EmoteListWidget->GetVisibility() == ESlateVisibility::Visible;
	UpdateCursorAndInputMode();
}


void UUIManager::InitializeNotificationSystem(UBestiaryNetworkHandler* InBestiaryHandler)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeNotificationSystem"));

	if (!InBestiaryHandler)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeNotificationSystem - BestiaryHandler is null"));
		return;
	}

	// Create bestiary widget
	if (BestiaryWidgetClass && !BestiaryWidget)
	{
		BestiaryWidget = CreateWidget<UBestiaryWidget>(GetWorld(), BestiaryWidgetClass);
		if (BestiaryWidget)
		{
			BestiaryWidget->AddToViewport(90);
			BestiaryWidget->SetVisibility(ESlateVisibility::Collapsed);
			BestiaryWidget->BindToBestiaryHandler(InBestiaryHandler);
			BestiaryWidget->OnBestiaryVisibilityChanged.AddDynamic(this, &UUIManager::OnBestiaryVisibilityChanged);
			UE_LOG(LogTemp, Log, TEXT("UIManager: BestiaryWidget created and bound"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create BestiaryWidget"));
		}
	}
	else if (!BestiaryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: BestiaryWidgetClass is not set in Blueprint"));
	}

	// Create chat widget if ChatManager is available through GameInstance
	if (ChatWidgetClass && !ChatWidget)
	{
		if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
		{
			UChatManager* ChatMgr = GameInstance->GetChatManager();
			if (ChatMgr)
			{
				ChatWidget = CreateWidget<UChatWidget>(GetWorld(), ChatWidgetClass);
				if (ChatWidget)
				{
					ChatWidget->AddToViewport(15); // Low Z-order, always visible as HUD overlay
					ChatWidget->InitializeChatWidget(ChatMgr);
					UE_LOG(LogTemp, Log, TEXT("UIManager: ChatWidget created and bound to ChatManager"));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create ChatWidget"));
				}
			}
		}
	}
	else if (!ChatWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: ChatWidgetClass is not set in Blueprint"));
	}

	// ====================================================================
	// World Notification Widgets
	// ====================================================================

	// Toast widget (top-right corner pop-ups for medium-priority events)
	if (NotificationToastWidgetClass && !NotificationToastWidget)
	{
		NotificationToastWidget = CreateWidget<UNotificationToastWidget>(GetWorld(), NotificationToastWidgetClass);
		if (NotificationToastWidget)
		{
			NotificationToastWidget->AddToViewport(50);
			UE_LOG(LogTemp, Log, TEXT("UIManager: NotificationToastWidget created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create NotificationToastWidget"));
		}
	}
	else if (!NotificationToastWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: NotificationToastWidgetClass is not set in Blueprint"));
	}

	// Zone banner widget (large banner for zone transitions)
	if (NotificationZoneBannerWidgetClass && !NotificationZoneBannerWidget)
	{
		NotificationZoneBannerWidget = CreateWidget<UNotificationZoneBannerWidget>(GetWorld(), NotificationZoneBannerWidgetClass);
		if (NotificationZoneBannerWidget)
		{
			NotificationZoneBannerWidget->AddToViewport(55);
			UE_LOG(LogTemp, Log, TEXT("UIManager: NotificationZoneBannerWidget created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create NotificationZoneBannerWidget"));
		}
	}
	else if (!NotificationZoneBannerWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: NotificationZoneBannerWidgetClass is not set in Blueprint"));
	}

	// Screen center widget (full-screen flash for critical events like level_up)
	if (NotificationScreenCenterWidgetClass && !NotificationScreenCenterWidget)
	{
		NotificationScreenCenterWidget = CreateWidget<UNotificationScreenCenterWidget>(GetWorld(), NotificationScreenCenterWidgetClass);
		if (NotificationScreenCenterWidget)
		{
			NotificationScreenCenterWidget->AddToViewport(60);
			UE_LOG(LogTemp, Log, TEXT("UIManager: NotificationScreenCenterWidget created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create NotificationScreenCenterWidget"));
		}
	}
	else if (!NotificationScreenCenterWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: NotificationScreenCenterWidgetClass is not set in Blueprint"));
	}

	// Atmosphere widget (semi-transparent ambient text)
	if (NotificationAtmosphereWidgetClass && !NotificationAtmosphereWidget)
	{
		NotificationAtmosphereWidget = CreateWidget<UNotificationAtmosphereWidget>(GetWorld(), NotificationAtmosphereWidgetClass);
		if (NotificationAtmosphereWidget)
		{
			NotificationAtmosphereWidget->AddToViewport(45);
			UE_LOG(LogTemp, Log, TEXT("UIManager: NotificationAtmosphereWidget created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create NotificationAtmosphereWidget"));
		}
	}
	else if (!NotificationAtmosphereWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: NotificationAtmosphereWidgetClass is not set in Blueprint"));
	}

	// ====================================================================
	// WorldNotificationManager — routes world_notification packets to widgets
	// ====================================================================
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
	{
		UNetworkManager* NetMgr = GI->GetNetworkManager();
		if (NetMgr)
		{
			if (!WorldNotificationManager)
			{
				WorldNotificationManager = NewObject<UWorldNotificationManager>(this);
			}

			WorldNotificationManager->Initialize(
				GI,
				NetMgr,
				InBestiaryHandler,
				NotificationToastWidget,
				NotificationZoneBannerWidget,
				NotificationScreenCenterWidget,
				NotificationAtmosphereWidget,
				BestiaryWidget);

			WorldNotificationManager->SubscribeToNetworkEvents();
			UE_LOG(LogTemp, Log, TEXT("UIManager: WorldNotificationManager created and subscribed to network events"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: NetworkManager is null, WorldNotificationManager not initialized"));
		}
	}
}

void UUIManager::CreateExperienceWidget()
{
	// Experience widget is now managed by PlayerInterfaceWidget
	// This method is kept for compatibility but delegates to PlayerInterfaceWidget
	UE_LOG(LogTemp, Log, TEXT("UIManager::CreateExperienceWidget - handled by PlayerInterfaceWidget"));
}

void UUIManager::OnMenuBarBestiaryClicked()
{
	ToggleBestiary();
}

void UUIManager::OnMenuBarTitlesClicked()
{
	ToggleTitles();
}

void UUIManager::OnMenuBarReputationClicked()
{
	ToggleReputation();
}

void UUIManager::OnMenuBarEmoteClicked()
{
	ToggleEmoteList();
}

void UUIManager::HandleGameMenuResumeClicked()
{
	if (GameMenuWidget) { GameMenuWidget->CloseMenu(); }
	bGameMenuVisible = false;
	UpdateCursorAndInputMode();
}

void UUIManager::HandleSettingsClicked()
{
	if (!GameSettingsWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::HandleSettingsClicked - GameSettingsWidgetClass not set in Blueprint"));
		return;
	}

	// Create once, reuse on subsequent calls.
	if (!IsValid(GameSettingsWidget))
	{
		GameSettingsWidget = CreateWidget<UW_SettingsWidget>(GetWorld(), GameSettingsWidgetClass);
		if (GameSettingsWidget)
		{
			// Z-order 210 — same level as the old audio settings widget, above the game menu (200).
			GameSettingsWidget->AddToViewport(210);
			UE_LOG(LogTemp, Log, TEXT("UIManager: GameSettingsWidget created"));
		}
	}

	if (GameSettingsWidget)
	{
		GameSettingsWidget->OpenSettings();
	}
}

void UUIManager::HandleAudioSettingsClosed()
{
	if (AudioSettingsWidget)
		AudioSettingsWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UUIManager::HandleExitToLoginClicked()
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::HandleExitToLoginClicked"));
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
	{
		GI->ReturnToLoginLevel();
	}
}

void UUIManager::HandleExitToDesktopClicked()
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::HandleExitToDesktopClicked"));
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}

// GameMenuBar button handlers
void UUIManager::OnMenuBarInventoryClicked()    { ToggleInventory(); }
void UUIManager::OnMenuBarEquipmentClicked()    { ToggleEquipment(); }
void UUIManager::OnMenuBarQuestJournalClicked() { ToggleQuestJournal(); }
void UUIManager::OnMenuBarSkillsClicked()       { ToggleSkillsPanel(); }
void UUIManager::OnMenuBarStatsClicked()        { TogglePlayerStats(); }
void UUIManager::OnMenuBarMenuClicked()
{
    // Menu bar button: just toggle the game menu, do NOT close other open windows.
    // (Escape key calls ToggleGameMenu() which has the WoW-style "close panels first" behaviour.)
    if (!GameMenuWidget) return;

    if (GameMenuWidget->IsMenuOpen())
    {
        GameMenuWidget->CloseMenu();
        bGameMenuVisible = false;
    }
    else
    {
        GameMenuWidget->OpenMenu();
        bGameMenuVisible = true;
    }
    UpdateCursorAndInputMode();
}

void UUIManager::ToggleEquipment()
{
	if (!EquipmentWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::ToggleEquipment - EquipmentWidget not initialized"));
		return;
	}

	EquipmentWidget->ToggleEquipment();
	UE_LOG(LogTemp, Log, TEXT("UIManager::ToggleEquipment - Toggled"));
}

void UUIManager::ToggleAltCursor()
{
	bAltCursorActive = !bAltCursorActive;
	UpdateCursorAndInputMode();
	UE_LOG(LogTemp, Log, TEXT("UIManager::ToggleAltCursor - %s"), bAltCursorActive ? TEXT("Active") : TEXT("Inactive"));
}

void UUIManager::TogglePlayerStats()
{
	if (!PlayerStatsWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::TogglePlayerStats - PlayerStatsWidget not initialized (assign PlayerStatsWidgetClass in Blueprint)"));
		return;
	}

	PlayerStatsWidget->ToggleStats();
	UE_LOG(LogTemp, Log, TEXT("UIManager::TogglePlayerStats - Toggled"));
}

void UUIManager::ToggleBestiary()
{
	if (!BestiaryWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::ToggleBestiary - BestiaryWidget not initialized (assign BestiaryWidgetClass in Blueprint)"));
		return;
	}

	BestiaryWidget->ToggleBestiary();
	UE_LOG(LogTemp, Log, TEXT("UIManager::ToggleBestiary - Toggled"));
}

void UUIManager::ToggleTitles()
{
	if (!TitlesWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::ToggleTitles - TitlesWidget not initialized (assign TitlesWidgetClass in Blueprint)"));
		return;
	}

	TitlesWidget->ToggleTitles();
	UE_LOG(LogTemp, Log, TEXT("UIManager::ToggleTitles - Toggled"));
}

void UUIManager::ToggleReputation()
{
	if (!ReputationWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::ToggleReputation - ReputationWidget not initialized (assign ReputationWidgetClass in Blueprint)"));
		return;
	}

	ReputationWidget->ToggleReputation();
	UE_LOG(LogTemp, Log, TEXT("UIManager::ToggleReputation - Toggled"));
}

void UUIManager::ToggleEmoteList()
{
	if (!EmoteListWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::ToggleEmoteList - EmoteListWidget not initialized (assign EmoteListWidgetClass in Blueprint)"));
		return;
	}
	EmoteListWidget->ToggleEmoteList();
	UE_LOG(LogTemp, Log, TEXT("UIManager::ToggleEmoteList - Toggled"));
}

void UUIManager::ToggleQuestJournal()
{
	if (!QuestJournalWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::ToggleQuestJournal - QuestJournalWidget not initialized"));
		return;
	}

	QuestJournalWidget->ToggleJournal();
	UE_LOG(LogTemp, Log, TEXT("UIManager::ToggleQuestJournal - Toggled"));
}

void UUIManager::ShowDeathScreen(int32 RespawnTimeSec)
{
	UE_LOG(LogTemp, Warning, TEXT("UIManager::ShowDeathScreen RespawnTime=%d"), RespawnTimeSec);

	// If PlayerController is not yet assigned, defer the death screen until InitFTCManager is called
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::ShowDeathScreen - PlayerController not ready, deferring"));
		bPendingDeathScreen = true;
		PendingDeathScreenDebt = RespawnTimeSec;
		return;
	}

	// Create widget lazily if not yet created
	if (!DeathScreenWidget && DeathScreenWidgetClass)
	{
		DeathScreenWidget = CreateWidget<UDeathScreenWidget>(PlayerController, DeathScreenWidgetClass);
		if (DeathScreenWidget)
		{
			DeathScreenWidget->AddToViewport(100);

			// Wire the respawn button to the owning ABasicPlayer.
			// UIManager is an ActorComponent whose owner is ABasicPlayer.
			if (ABasicPlayer* OwnerPlayer = Cast<ABasicPlayer>(GetOwner()))
			{
				DeathScreenWidget->OnRespawnRequested.AddDynamic(OwnerPlayer, &ABasicPlayer::OnRespawnClicked);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UIManager::ShowDeathScreen - owner is not ABasicPlayer, respawn button will not work"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager::ShowDeathScreen - failed to create DeathScreenWidget"));
			return;
		}
	}

	if (DeathScreenWidget)
	{
		DeathScreenWidget->ShowDeathScreen(RespawnTimeSec);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::ShowDeathScreen - DeathScreenWidgetClass not set in Blueprint"));
	}
}

void UUIManager::HideDeathScreen()
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::HideDeathScreen"));

	if (DeathScreenWidget)
	{
		DeathScreenWidget->HideDeathScreen();
	}
}

void UUIManager::UpdateMobTargetFrameHP(int32 CurrentHP, int32 MaxHP)
{
	if (PlayerInterfaceWidget)
	{
		UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget();
		if (TargetFrame)
		{
			TargetFrame->UpdateHP(CurrentHP, MaxHP);
		}
	}
}

void UUIManager::ShowHealScreenFlash()
{
	UE_LOG(LogTemp, Verbose, TEXT("UIManager::ShowHealScreenFlash"));
	EnsureFlashWidget();
	if (CombatScreenFlashWidget)
	{
		CombatScreenFlashWidget->PlayHealFlash();
	}
}

void UUIManager::ShowDamageScreenFlash()
{
	UE_LOG(LogTemp, Verbose, TEXT("UIManager::ShowDamageScreenFlash"));
	EnsureFlashWidget();
	if (CombatScreenFlashWidget)
	{
		CombatScreenFlashWidget->PlayDamageFlash();
	}
}

void UUIManager::SetLowHealthWarning(bool bActive)
{
	EnsureFlashWidget();
	if (CombatScreenFlashWidget)
	{
		CombatScreenFlashWidget->SetLowHealthWarning(bActive);
	}
}

void UUIManager::EnsureFlashWidget()
{
	if (CombatScreenFlashWidget && CombatScreenFlashWidget->IsInViewport()) return;

	TSubclassOf<UCombatScreenFlashWidget> WidgetClass = CombatScreenFlashWidgetClass
		? CombatScreenFlashWidgetClass
		: TSubclassOf<UCombatScreenFlashWidget>(UCombatScreenFlashWidget::StaticClass());

	if (!GetWorld()) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	CombatScreenFlashWidget = CreateWidget<UCombatScreenFlashWidget>(PC, WidgetClass);
	if (CombatScreenFlashWidget)
	{
		// ZOrder 5 keeps it behind all HUD/UI elements (lowest HUD is PlayerInterface at 10)
		CombatScreenFlashWidget->AddToViewport(5);
	}
}

// ============================================================================
// WIO (World Interactive Objects) Widget Management
// ============================================================================

void UUIManager::InitializeWIOWidgets(UWorldObjectManager* InWorldObjectManager)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeWIOWidgets - Starting initialization"));

	if (!InWorldObjectManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeWIOWidgets - WorldObjectManager is null"));
		return;
	}

	// Create interaction prompt widget
	if (WIOInteractionPromptWidgetClass && !WIOInteractionPromptWidget)
	{
		WIOInteractionPromptWidget = CreateWidget<UWIOInteractionPromptWidget>(GetWorld(), WIOInteractionPromptWidgetClass);
		if (WIOInteractionPromptWidget)
		{
			WIOInteractionPromptWidget->AddToViewport(60);
			WIOInteractionPromptWidget->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Log, TEXT("UIManager: WIOInteractionPromptWidget created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create WIOInteractionPromptWidget"));
		}
	}
	else if (!WIOInteractionPromptWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: WIOInteractionPromptWidgetClass is not set in Blueprint"));
	}

	// Create channel bar widget
	if (WIOChannelBarWidgetClass && !WIOChannelBarWidget)
	{
		WIOChannelBarWidget = CreateWidget<UWIOChannelBarWidget>(GetWorld(), WIOChannelBarWidgetClass);
		if (WIOChannelBarWidget)
		{
			WIOChannelBarWidget->AddToViewport(65);
			WIOChannelBarWidget->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Log, TEXT("UIManager: WIOChannelBarWidget created"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to create WIOChannelBarWidget"));
		}
	}
	else if (!WIOChannelBarWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: WIOChannelBarWidgetClass is not set in Blueprint"));
	}

	// Bind to WorldObjectManager delegates for automatic UI updates
	InWorldObjectManager->OnInteractResult.AddDynamic(this, &UUIManager::HandleWIOInteractResult);
	InWorldObjectManager->OnChannelCancelled.AddDynamic(this, &UUIManager::HandleWIOChannelCancelled);

	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeWIOWidgets - Completed"));
}

void UUIManager::ShowWIOInteractionPrompt(int32 ObjectId)
{
	if (!WIOInteractionPromptWidget) return;

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld()->GetGameInstance());
	if (!GI) return;

	UWorldObjectManager* WOM = GI->GetWorldObjectManager();
	if (!WOM) return;

	AWorldInteractiveObjectActor* Actor = WOM->GetObjectActorById(ObjectId);
	if (!Actor) return;

	WIOInteractionPromptWidget->ShowForObject(Actor);
	WIOInteractionPromptWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UUIManager::HideWIOInteractionPrompt()
{
	if (!WIOInteractionPromptWidget) return;

	WIOInteractionPromptWidget->HidePrompt();
	WIOInteractionPromptWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UUIManager::ShowWIOChannelBar(int32 ObjectId, float Duration)
{
	if (!WIOChannelBarWidget) return;

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld()->GetGameInstance());
	if (!GI || !GI->GetWorldObjectManager()) return;

	// Get object data for display name
	FText ObjectName = FText::FromString(TEXT("Channeling..."));
	if (UWorldObjectManager* WOM = GI->GetWorldObjectManager())
	{
		// Use localization for the display name if available
		if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
		{
			const TMap<int32, FWorldObjectData>& Registry = WOM->GetObjectDataRegistry();
			if (const FWorldObjectData* ObjData = Registry.Find(ObjectId))
			{
				FText LocName = Loc->GetWIODisplayName(ObjData->NameKey);
				if (!LocName.IsEmpty())
				{
					ObjectName = LocName;
				}
			}
		}
	}

	WIOChannelBarWidget->StartChannel(ObjectName, Duration);
	WIOChannelBarWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UUIManager::HideWIOChannelBar()
{
	if (!WIOChannelBarWidget) return;

	WIOChannelBarWidget->StopChannel();
	WIOChannelBarWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UUIManager::HandleWIOInteractResult(const FWIOInteractResult& Result)
{
	if (Result.bSuccess)
	{
		// If this is a channeled interaction starting, show the channel bar
		if (Result.InteractionType == TEXT("channeled") && Result.ChannelTimeSec > 0)
		{
			ShowWIOChannelBar(Result.ObjectId, static_cast<float>(Result.ChannelTimeSec));
			return;
		}

		// Channeled complete or non-channeled success — hide UI
		HideWIOInteractionPrompt();
		HideWIOChannelBar();
	}
}

void UUIManager::HandleWIOChannelCancelled(int32 ObjectId)
{
	HideWIOChannelBar();
}
