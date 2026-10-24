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

// S5: widget chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "UIManager"

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
	FText ObjectName = LOCTEXT("Channeling", "Channeling...");
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

void UUIManager::ShowIdleWarning(int32 TotalSecondsRemaining)
{
	if (IdleWarningWidget)
	{
		IdleWarningWidget->ShowIdleWarning(TotalSecondsRemaining);
	}
}

void UUIManager::HideIdleWarning()
{
	if (IdleWarningWidget)
	{
		IdleWarningWidget->HideIdleWarning();
	}
}

void UUIManager::UpdateIdleCountdown(int32 SecondsRemaining)
{
	if (IdleWarningWidget)
	{
		IdleWarningWidget->UpdateCountdown(SecondsRemaining);
	}
}

#undef LOCTEXT_NAMESPACE
