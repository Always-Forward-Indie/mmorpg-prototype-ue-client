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
	// Guard: allow only one toggle per frame to prevent double-fire when both
	// OnPreInputKeyDown (Editor) and Enhanced Input (Shipping) trigger in the same frame.
	if (AltToggledFrame == GFrameCounter) return;
	AltToggledFrame = GFrameCounter;

	bAltCursorActive = !bAltCursorActive;
	UpdateCursorAndInputMode();
	UE_LOG(LogTemp, Log, TEXT("UIManager::ToggleAltCursor - %s"), bAltCursorActive ? TEXT("Active") : TEXT("Inactive"));
}

void UUIManager::OnPreInputKeyDown(const FKeyEvent& InKeyEvent)
{
	if (!AltCursorAction || !PlayerController)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	const UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (Subsystem->QueryKeysMappedToAction(AltCursorAction).Contains(InKeyEvent.GetKey()))
	{
		ToggleAltCursor();
	}
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

