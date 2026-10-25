#include "UI/UIManager.h"
#include "Prototyping.h"
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

void UUIManager::InitializeTitlesWidget(UTitleManager* InTitleManager, UTitleNetworkHandler* InTitleHandler, int32 InCharacterId)
{
	UE_LOG(LogGameplay, Log, TEXT("UIManager::InitializeTitlesWidget"));

	if (!InTitleManager)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager::InitializeTitlesWidget - TitleManager is null"));
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
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: TitlesWidget created and bound"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create TitlesWidget"));
		}
	}
	else if (!TitlesWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: TitlesWidgetClass is not set in Blueprint"));
	}
}

void UUIManager::InitializeReputationWidget(UReputationManager* InReputationManager)
{
	UE_LOG(LogGameplay, Log, TEXT("UIManager::InitializeReputationWidget"));

	if (!InReputationManager)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager::InitializeReputationWidget - ReputationManager is null"));
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
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: ReputationWidget created and bound"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create ReputationWidget"));
		}
	}
	else if (!ReputationWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: ReputationWidgetClass is not set in Blueprint"));
	}
}

void UUIManager::InitializeEmoteListWidget(UEmoteManager* InEmoteManager, UEmoteNetworkHandler* InEmoteHandler, int32 InCharacterId)
{
	UE_LOG(LogGameplay, Log, TEXT("UIManager::InitializeEmoteListWidget"));

	if (!InEmoteManager)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager::InitializeEmoteListWidget - EmoteManager is null"));
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
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: EmoteListWidget created and bound"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create EmoteListWidget"));
		}
	}
	else if (!EmoteListWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: EmoteListWidgetClass is not set in Blueprint"));
	}
}

void UUIManager::OnEmoteListVisibilityChanged()
{
	bEmoteListVisible = EmoteListWidget && EmoteListWidget->GetVisibility() == ESlateVisibility::Visible;
	UpdateCursorAndInputMode();
}


void UUIManager::InitializeNotificationSystem(UBestiaryNetworkHandler* InBestiaryHandler)
{
	UE_LOG(LogGameplay, Log, TEXT("UIManager::InitializeNotificationSystem"));

	if (!InBestiaryHandler)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager::InitializeNotificationSystem - BestiaryHandler is null"));
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
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: BestiaryWidget created and bound"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create BestiaryWidget"));
		}
	}
	else if (!BestiaryWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: BestiaryWidgetClass is not set in Blueprint"));
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
					UE_LOG(LogGameplay, Verbose, TEXT("UIManager: ChatWidget created and bound to ChatManager"));
				}
				else
				{
					UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create ChatWidget"));
				}
			}
		}
	}
	else if (!ChatWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: ChatWidgetClass is not set in Blueprint"));
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
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: NotificationToastWidget created"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create NotificationToastWidget"));
		}
	}
	else if (!NotificationToastWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: NotificationToastWidgetClass is not set in Blueprint"));
	}

	// Zone banner widget (large banner for zone transitions)
	if (NotificationZoneBannerWidgetClass && !NotificationZoneBannerWidget)
	{
		NotificationZoneBannerWidget = CreateWidget<UNotificationZoneBannerWidget>(GetWorld(), NotificationZoneBannerWidgetClass);
		if (NotificationZoneBannerWidget)
		{
			NotificationZoneBannerWidget->AddToViewport(55);
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: NotificationZoneBannerWidget created"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create NotificationZoneBannerWidget"));
		}
	}
	else if (!NotificationZoneBannerWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: NotificationZoneBannerWidgetClass is not set in Blueprint"));
	}

	// Screen center widget (full-screen flash for critical events like level_up)
	if (NotificationScreenCenterWidgetClass && !NotificationScreenCenterWidget)
	{
		NotificationScreenCenterWidget = CreateWidget<UNotificationScreenCenterWidget>(GetWorld(), NotificationScreenCenterWidgetClass);
		if (NotificationScreenCenterWidget)
		{
			NotificationScreenCenterWidget->AddToViewport(60);
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: NotificationScreenCenterWidget created"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create NotificationScreenCenterWidget"));
		}
	}
	else if (!NotificationScreenCenterWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: NotificationScreenCenterWidgetClass is not set in Blueprint"));
	}

	// Atmosphere widget (semi-transparent ambient text)
	if (NotificationAtmosphereWidgetClass && !NotificationAtmosphereWidget)
	{
		NotificationAtmosphereWidget = CreateWidget<UNotificationAtmosphereWidget>(GetWorld(), NotificationAtmosphereWidgetClass);
		if (NotificationAtmosphereWidget)
		{
			NotificationAtmosphereWidget->AddToViewport(45);
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: NotificationAtmosphereWidget created"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: Failed to create NotificationAtmosphereWidget"));
		}
	}
	else if (!NotificationAtmosphereWidgetClass)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: NotificationAtmosphereWidgetClass is not set in Blueprint"));
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
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: WorldNotificationManager created and subscribed to network events"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager: NetworkManager is null, WorldNotificationManager not initialized"));
		}
	}
}

void UUIManager::CreateExperienceWidget()
{
	// Experience widget is now managed by PlayerInterfaceWidget
	// This method is kept for compatibility but delegates to PlayerInterfaceWidget
	UE_LOG(LogGameplay, Log, TEXT("UIManager::CreateExperienceWidget - handled by PlayerInterfaceWidget"));
}

