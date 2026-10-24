#include "UI/UIManager.h"
#include "UI/UIManager.h"
#include "UI/InventoryWidget.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "UI/InteractionHintWidget.h"
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
#include "Gameplay/UI/GameVersionWidget.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Framework/Application/SlateApplication.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/PlatformProcess.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "UI/PlayerStatsWidget.h"
#include "UI/BestiaryWidget.h"
#include "UI/ChatWidget.h"
#include "UI/GameMenuBarWidget.h"
#include "UI/GameMenuWidget.h"
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
#include "UI/IdleWarningWidget.h"
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
	// bAltCursorActive = false starts with cursor hidden for free-look camera.
	// Alt key toggles cursor visibility; UI windows force it visible.
	bAltCursorActive = false;
	bGameMenuVisible = false;
	bDeathScreenVisible = false;
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

	// Unregister Slate pre-input listener (Editor-only API)
#if !UE_BUILD_SHIPPING
	if (PreInputKeyDownHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(PreInputKeyDownHandle);
		PreInputKeyDownHandle.Reset();
	}
#endif

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

void UUIManager::RegisterPreInputListener()
{
#if !UE_BUILD_SHIPPING
	if (PreInputKeyDownHandle.IsValid()) return;

	if (FSlateApplication::IsInitialized())
	{
		PreInputKeyDownHandle = FSlateApplication::Get().OnApplicationPreInputKeyDownListener().AddUObject(this, &UUIManager::OnPreInputKeyDown);
	}
#else
	// Shipping: OnApplicationPreInputKeyDownListener is not available.
	// Alt key is handled via Enhanced Input binding in BasicPlayer instead.
#endif
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
	FCTManager->MaxVisibleDistanceCm = FCTMaxVisibleDistanceCm;
	FCTManager->FadeStartDistanceCm = FCTFadeStartDistanceCm;

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

