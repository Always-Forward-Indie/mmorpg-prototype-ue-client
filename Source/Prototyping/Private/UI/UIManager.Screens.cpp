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

void UUIManager::ShowDeathScreen(int32 RespawnTimeSec)
{
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager::ShowDeathScreen RespawnTime=%d"), RespawnTimeSec);

	// If PlayerController is not yet assigned, defer the death screen until InitFTCManager is called
	if (!PlayerController)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager::ShowDeathScreen - PlayerController not ready, deferring"));
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
				UE_LOG(LogGameplay, Error, TEXT("UIManager::ShowDeathScreen - owner is not ABasicPlayer, respawn button will not work"));
			}
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("UIManager::ShowDeathScreen - failed to create DeathScreenWidget"));
			return;
		}
	}

	if (DeathScreenWidget)
	{
		DeathScreenWidget->ShowDeathScreen(RespawnTimeSec);
		bDeathScreenVisible = true;
		UpdateCursorAndInputMode();
	}
	else
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager::ShowDeathScreen - DeathScreenWidgetClass not set in Blueprint"));
	}
}

void UUIManager::HideDeathScreen()
{
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager::HideDeathScreen"));

	if (DeathScreenWidget)
	{
		DeathScreenWidget->HideDeathScreen();
		bDeathScreenVisible = false;
		UpdateCursorAndInputMode();
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
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager::ShowHealScreenFlash"));
	EnsureFlashWidget();
	if (CombatScreenFlashWidget)
	{
		CombatScreenFlashWidget->PlayHealFlash();
	}
}

void UUIManager::ShowDamageScreenFlash()
{
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager::ShowDamageScreenFlash"));
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

