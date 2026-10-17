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

