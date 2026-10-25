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
		UE_LOG(LogGameplay, Warning, TEXT("UIManager::HandleSettingsClicked - GameSettingsWidgetClass not set in Blueprint"));
		return;
	}

	// Create once, reuse on subsequent calls.
	if (!IsValid(GameSettingsWidget))
	{
		GameSettingsWidget = CreateWidget<UW_SettingsWidget>(GetWorld(), GameSettingsWidgetClass);
		if (GameSettingsWidget)
		{
			// Z-order 210 — above the game menu (200).
			GameSettingsWidget->AddToViewport(210);
			GameSettingsWidget->OnSettingsWindowClosed.AddDynamic(this, &UUIManager::HandleSettingsClosed);
			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: GameSettingsWidget created"));
		}
	}

	if (GameSettingsWidget)
	{
		GameSettingsWidget->OpenSettings();
	}
}

void UUIManager::HandleSettingsClosed()
{
	UpdateCursorAndInputMode();
}

void UUIManager::HandleExitToLoginClicked()
{
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager::HandleExitToLoginClicked"));
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
	{
		GI->ReturnToLoginLevel();
	}
}

void UUIManager::HandleExitToDesktopClicked()
{
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager::HandleExitToDesktopClicked"));
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}

void UUIManager::HandleBugReportClicked()
{
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager::HandleBugReportClicked"));
	if (!BugReportUrl.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*BugReportUrl, nullptr, nullptr);
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

