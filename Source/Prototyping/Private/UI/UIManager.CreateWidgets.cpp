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
			HarvestProgressWidget->AddToViewport(100);
			
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
			HarvestLootWidget->AddToViewport(100);
			
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

void UUIManager::CreateInteractionHintWidget()
{
	if (InteractionHintWidgetClass)
	{
		InteractionHintWidget = CreateWidget<UInteractionHintWidget>(GetWorld(), InteractionHintWidgetClass);
		if (InteractionHintWidget)
		{
			InteractionHintWidget->AddToViewport(5);
			InteractionHintWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UUIManager::ShowInteractionHint(EInteractableType Type)
{
	if (Type == EInteractableType::None)
	{
		HideInteractionHint();
		return;
	}

	bHoverHintActive = true;
	if (InteractionHintWidget)
	{
		InteractionHintWidget->ShowHint(Type, false);
	}
}

void UUIManager::HideInteractionHint()
{
	bHoverHintActive = false;
	if (bProximityHintActive)
	{
		return;
	}
	if (InteractionHintWidget)
	{
		InteractionHintWidget->HideHint();
	}
}

void UUIManager::ShowProximityHint(EInteractableType Type)
{
	bProximityHintActive = true;
	if (bHoverHintActive || !InteractionHintWidget)
	{
		return;
	}
	InteractionHintWidget->ShowHint(Type);
}

void UUIManager::ClearProximityHint()
{
	bProximityHintActive = false;
	if (bHoverHintActive || !InteractionHintWidget)
	{
		return;
	}
	InteractionHintWidget->HideHint();
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
	GameVersionWidget = CreateWidget<UGameVersionWidget>(GetWorld(), GameVersionWidgetClass);
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
	GameMenuWidget->OnBugReportClicked.AddDynamic(this, &UUIManager::HandleBugReportClicked);

	UE_LOG(LogTemp, Warning, TEXT("UIManager: GameMenuWidget created and wired (class: %s)"),
		*GameMenuWidgetClass->GetName());
}

