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


