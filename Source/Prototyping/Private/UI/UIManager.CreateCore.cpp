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
#include "UI/IdleWarningWidget.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Services/LocalizationSubsystem.h"

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

	// Create interaction hint widget
	CreateInteractionHintWidget();

	// Create skill widgets (now handled by PlayerInterfaceWidget)
	CreateSkillWidgets();

	// Create game version widget
	CreateGameVersionWidget();

	// Create game menu bar (always-visible bottom bar)
	CreateGameMenuBarWidget();

	// Create game menu (opened on Escape)
	CreateGameMenuWidget();
	
	// Create idle warning widget (hidden by default, shown by IdleTimeoutManager)
	if (IdleWarningWidgetClass)
	{
		IdleWarningWidget = CreateWidget<UIdleWarningWidget>(GetWorld(), IdleWarningWidgetClass);
		if (IdleWarningWidget)
		{
			IdleWarningWidget->AddToViewport(70);
		}
	}
	
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

