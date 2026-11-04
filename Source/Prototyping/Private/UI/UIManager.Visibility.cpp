#include "UI/UIManager.h"
#include "Prototyping.h"
#include "Utils/NetCore/WindowVisibilityRule.h"
#include "Blueprint/WidgetTree.h"
#include "UI/InventoryWidget.h"
#include "UI/ChatWidget.h"
#include "UI/CombatLogWidget.h"
#include "UI/PlayerInspectWidget.h"
#include "UI/TradeInviteWidget.h"
#include "UI/PlayerContextMenuWidget.h"
#include "UI/GameMenuWidget.h"
#include "UI/GameMenuBarWidget.h"
#include "UI/W_SettingsWidget.h"
#include "UI/KeybindWidget.h"
#include "Gameplay/UI/DeathScreenWidget.h"
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

// Window widgets use a mix of Visible / SelfHitTestInvisible / HitTestInvisible
// (set by the widget or its Blueprint), so "open" must never be tested by
// comparing against one exact enum value — that silently desyncs the flag
// (e.g. stats set Visible while the check wanted SelfHitTestInvisible) and
// breaks everything downstream (camera zoom, click-through, cursor mode).
static bool IsEffectivelyVisible(const UWidget* Widget)
{
	return IsValid(Widget)
		&& Widget->GetVisibility() != ESlateVisibility::Collapsed
		&& Widget->GetVisibility() != ESlateVisibility::Hidden;
}
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

void UUIManager::OnAvailableSkillsVisibilityChanged(bool bIsVisible)
{
	// �������������� ��������� ��������� � ���������� ���������� �������
	bSkillsPanelVisible = bIsVisible;
	
	// ��������� ������ � ����� �����
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Skills panel visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::HandlePlayerInterfaceReady()
{
	if (PlayerInterfaceWidget)
	{
		PlayerInterfaceWidget->OnPlayerInterfaceReady.RemoveDynamic(this, &UUIManager::HandlePlayerInterfaceReady);
	}

	// Create NameplateManager and bind to the canvas widget from PlayerInterfaceWidget
	if (!NameplateManager && PlayerInterfaceWidget)
	{
		NameplateManager = NewObject<UNameplateManager>(this);
		if (NameplateManager)
		{
			NameplateManager->RegisterComponent();
			UNameplateCanvasWidget* Canvas = PlayerInterfaceWidget->GetNameplateCanvasWidget();
			if (Canvas)
			{
				NameplateManager->SetCanvasWidget(Canvas);
				UE_LOG(LogGameplay, Verbose, TEXT("UIManager: NameplateManager created and bound to canvas widget"));
			}
			else
			{
				UE_LOG(LogGameplay, Warning, TEXT("UIManager: NameplateManager created but NameplateCanvasWidget not found in PlayerInterfaceWidget. Add it to the WBP_PlayerInterface Blueprint."));
			}
		}
	}

	OnUIManagerInitialized.Broadcast();

	// Initialize item quickbar if the widget is present in the PlayerInterface
	if (PlayerInterfaceWidget && InventoryManager)
	{
		if (UItemQuickBarWidget* QuickBar = PlayerInterfaceWidget->GetItemQuickBar())
		{
			QuickBar->InitQuickBar(InventoryManager);
			InventoryManager->OnInventoryUpdated.AddDynamic(QuickBar, &UItemQuickBarWidget::HandleInventoryUpdated);
			UE_LOG(LogGameplay, Warning, TEXT("UIManager: ItemQuickBar initialized"));
		}
	}

	// Fill empty button click/hover sounds across the whole static UI tree.
	// Dynamic rows created later are swept on window-open transitions.
	ApplyButtonSoundsToAllUI();

	UE_LOG(LogGameplay, Warning, TEXT("[LOADSEQ] HandlePlayerInterfaceReady: OnUIManagerInitialized broadcast"));
}

void UUIManager::OnInventoryVisibilityChanged(bool bIsVisible)
{
	bInventoryVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Inventory visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnHarvestLootVisibilityChanged(bool bIsVisible)
{
	bHarvestLootVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Harvest loot visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnDialogueVisibilityChanged(bool bIsVisible)
{
	bDialogueVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Dialogue visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnQuestJournalVisibilityChanged(bool bIsVisible)
{
	bQuestJournalVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Quest journal visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnVendorShopVisibilityChanged(bool bIsVisible)
{
	bVendorShopVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Vendor shop visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnRepairShopVisibilityChanged(bool bIsVisible)
{
	bRepairShopVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Repair shop visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnSkillShopVisibilityChanged(bool bIsVisible)
{
	bSkillShopVisible = bIsVisible;
	UpdateCursorAndInputMode();

	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Skill shop visibility synced: %s"),
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnTradeVisibilityChanged(bool bIsVisible)
{
	bTradeVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Trade visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnEquipmentVisibilityChanged(bool bIsVisible)
{
	bEquipmentVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Equipment visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnPlayerStatsVisibilityChanged()
{
	bPlayerStatsVisible = IsEffectivelyVisible(PlayerStatsWidget);
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Player stats visibility synced: %s"), 
		bPlayerStatsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnTitlesVisibilityChanged()
{
	bTitlesVisible = IsEffectivelyVisible(TitlesWidget);
	UpdateCursorAndInputMode();
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Titles visibility synced: %s"), bTitlesVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnReputationVisibilityChanged()
{
	bReputationVisible = IsEffectivelyVisible(ReputationWidget);
	UpdateCursorAndInputMode();
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Reputation visibility synced: %s"), bReputationVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::OnBestiaryVisibilityChanged(bool bIsVisible)
{
	bBestiaryVisible = bIsVisible;
	UpdateCursorAndInputMode();
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Bestiary visibility synced: %s"), 
		bIsVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UUIManager::UpdateCursorAndInputMode()
{
	if (!PlayerController)
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: Cannot update cursor - PlayerController not set"));
		return;
	}

	bool bShouldShow = ShouldShowCursor();

	if (PlayerController->bShowMouseCursor != bShouldShow)
	{
		PlayerController->bShowMouseCursor = bShouldShow;

		if (bShouldShow)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
			InputMode.SetHideCursorDuringCapture(false);
			PlayerController->SetInputMode(InputMode);

			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Cursor shown - Game+UI mode"));
		}
		else
		{
			FInputModeGameOnly InputMode;
			PlayerController->SetInputMode(InputMode);

			UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Cursor hidden - Game only mode"));
		}
	}

	// Window open/close earcons: every visibility mutation funnels through here,
	// so a mask diff catches all windows in one spot (Alt-only toggles: no diff,
	// no sound). Opening a window also sweeps button sounds over the fresh rows.
	const uint32 WindowMask = BuildVisibilityMask();
	if (WindowMask != LastWindowMaskForSound)
	{
		const bool bOpened = (WindowMask & ~LastWindowMaskForSound) != 0;
		LastWindowMaskForSound = WindowMask;
		if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld()->GetGameInstance()))
		{
			if (GI->AudioManager)
			{
				GI->AudioManager->PlayUISound(bOpened ? EUISoundEvent::WindowOpen : EUISoundEvent::WindowClose);
			}
		}
		if (bOpened)
		{
			ApplyButtonSoundsToAllUI();
		}
	}

	// Always clear keyboard focus so SpaceBar/Enter don't trigger
	// focused UI button click handlers instead of game actions.
	// SetInputMode is guarded above; focus operations alone don't cause stutter.
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

bool UUIManager::HasUIWindowOpen() const
{
	// True only when an actual panel window is open and consuming cursor input.
	// Intentionally excludes bAltCursorActive so world interaction is never blocked
	// just because the cursor is always shown. Policy lives in FWindowVisibilityRule.
	return FWindowVisibilityRule::HasUIWindowOpen(BuildVisibilityMask());
}

uint32 UUIManager::BuildVisibilityMask() const
{
	using R = FWindowVisibilityRule;
	uint32 Flags = 0;
	if (bInventoryVisible)    Flags |= R::Inventory;
	if (bSkillsPanelVisible)  Flags |= R::SkillsPanel;
	if (bHarvestLootVisible)  Flags |= R::HarvestLoot;
	if (bDialogueVisible)     Flags |= R::Dialogue;
	if (bQuestJournalVisible) Flags |= R::QuestJournal;
	if (bVendorShopVisible)   Flags |= R::VendorShop;
	if (bRepairShopVisible)   Flags |= R::RepairShop;
	if (bSkillShopVisible)    Flags |= R::SkillShop;
	if (bTradeVisible)        Flags |= R::Trade;
	if (bEquipmentVisible)    Flags |= R::Equipment;
	if (bPlayerStatsVisible)  Flags |= R::PlayerStats;
	if (bBestiaryVisible)     Flags |= R::Bestiary;
	if (bTitlesVisible)       Flags |= R::Titles;
	if (bReputationVisible)   Flags |= R::Reputation;
	if (bEmoteListVisible)    Flags |= R::EmoteList;
	return Flags;
}

bool UUIManager::HasModalWindowOpen() const
{
	// Only windows that should fully block world interaction.
	// Non-modal windows (inventory, stats, skills, etc.) let UMG hit-testing
	// handle click-through: clicks on UI elements are consumed, clicks on
	// transparent/empty areas pass through to the game world.
	return FWindowVisibilityRule::HasModalWindowOpen(BuildVisibilityMask());
}

bool UUIManager::HasCursorOverWindowContent() const
{
	return FindBlockingWidget() != nullptr;
}

// Geometry walk (see FindBlockingWidget): any Visible widget under the cursor
// blocks, including passive Border/Image/backgrounds. SelfHitTestInvisible
// containers (incl. HUD instance wrappers) are transparent to this walk.
bool UUIManager::DoesWidgetTreeHaveHoveredChild(UWidget* Widget, const FVector2f& MousePos)
{
	if (!Widget || !Widget->IsVisible()) return false;

	// Only EVisibility::Visible widgets can block — SelfHitTestInvisible pass through
	if (Widget->GetVisibility() == ESlateVisibility::Visible &&
		Widget->GetCachedGeometry().IsUnderLocation(FVector2D(MousePos)))
		return true;

	// Traverse into UUserWidget's widget tree (InventoryWidget, DialogueWidget, etc.)
	if (UUserWidget* UserW = Cast<UUserWidget>(Widget))
	{
		if (UserW->WidgetTree && UserW->WidgetTree->RootWidget)
		{
			if (DoesWidgetTreeHaveHoveredChild(UserW->WidgetTree->RootWidget, MousePos))
				return true;
		}
	}

	// Traverse panel children (CanvasPanel, VerticalBox, etc.)
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			if (DoesWidgetTreeHaveHoveredChild(Panel->GetChildAt(i), MousePos))
				return true;
		}
	}

	// Traverse single-child content (Border, SizeBox, Button, etc.)
	if (UContentWidget* Content = Cast<UContentWidget>(Widget))
	{
		if (DoesWidgetTreeHaveHoveredChild(Content->GetContent(), MousePos))
			return true;
	}
	return false;
}

const UWidget* UUIManager::FindBlockingWidget() const
{
	if (!FSlateApplication::IsInitialized()) return nullptr;
	const FVector2f MousePos = FSlateApplication::Get().GetCursorPos();

	// Geometry walk (not Slate IsHovered): passive visuals (Border, Image,
	// backgrounds) never get a hover flag but DO eat clicks — and the camera
	// must stop everywhere a click would be consumed. Instance wrappers are
	// SelfHitTestInvisible, so fullscreen roots pass through here.
	auto Hit = [&MousePos = MousePos](const UWidget* W) -> const UWidget*
	{
		UWidget* Mutable = const_cast<UWidget*>(W);
		return DoesWidgetTreeHaveHoveredChild(Mutable, MousePos) ? W : nullptr;
	};
	auto Flagged = [&Hit](bool bFlag, const UWidget* W) -> const UWidget*
	{
		return (bFlag && W) ? Hit(W) : nullptr;
	};

	// Panel windows (flag-gated).
	const UWidget* Roots[] = {
		Flagged(bInventoryVisible, InventoryWidget),
		Flagged(bSkillsPanelVisible, AvailableSkillsWidget),
		Flagged(bHarvestLootVisible, HarvestLootWidget),
		Flagged(bDialogueVisible, DialogueWidget),
		Flagged(bQuestJournalVisible, QuestJournalWidget),
		Flagged(bVendorShopVisible, VendorShopWidget),
		Flagged(bRepairShopVisible, RepairShopWidget),
		Flagged(bSkillShopVisible, SkillShopWidget),
		Flagged(bTradeVisible, TradeWidget),
		Flagged(bEquipmentVisible, EquipmentWidget),
		Flagged(bPlayerStatsVisible, PlayerStatsWidget),
		Flagged(bBestiaryVisible, BestiaryWidget),
		Flagged(bTitlesVisible, TitlesWidget),
		Flagged(bReputationVisible, ReputationWidget),
		Flagged(bEmoteListVisible, EmoteListWidget),
		// Always-on HUD roots + dialogs (their instances are
		// SelfHitTestInvisible: only genuinely Visible content matches).
		Hit(ChatWidget),
		Hit(CombatLogWidget),
		Hit(PlayerInterfaceWidget),
		Hit(QuestTrackerWidget),
		Hit(TradeInviteWidget),
		Hit(PlayerContextMenu),
		Hit(PlayerInspectWidget),
		Hit(GameMenuWidget),
		Hit(GameMenuBarWidget),
		Hit(GameSettingsWidget),
		Hit(KeybindWidget),
		Hit(DeathScreenWidget),
	};
	for (const UWidget* W : Roots)
	{
		if (W)
		{
			LogHoverBlocker(W);
			return W;
		}
	}
	LogHoverBlocker(nullptr);
	return nullptr;
}

void UUIManager::LogHoverBlocker(const UWidget* Blocker) const
{
	// Dev diagnostic for the "camera stuck / clicks eaten" class: logs WHAT
	// the registry sees under the cursor (registry hit + Slate's own hover).
	// Throttled: on identity change, otherwise at most once per 5 s.
	const double Now = FPlatformTime::Seconds();
	FString Key;
	if (Blocker)
	{
		const FVector2D Sz = Blocker->GetCachedGeometry().GetAbsoluteSize();
		Key = FString::Printf(TEXT("%s [%s] vis=%d %.0fx%.0f"),
			*Blocker->GetPathName(), *Blocker->GetClass()->GetName(),
			(int32)Blocker->GetVisibility(), Sz.X, Sz.Y);
	}
	else
	{
		Key = TEXT("<none>");
	}
	const bool bChanged = !Key.Equals(LastHoverBlockerKey);
	if (!bChanged && Now - LastHoverBlockerTime < 5.0)
		return;
	LastHoverBlockerKey = Key;
	LastHoverBlockerTime = Now;

	// Slate truth: which cached widgets report IsHovered (real hit-testing,
	// not the manual geometry walk). Bounded to 12 entries.
	FString SlateHits;
	int32 SlateCount = 0;
	auto Accumulate = [&](const UWidget* Root)
	{
		TArray<const UWidget*> Stack;
		if (Root) Stack.Add(Root);
		while (Stack.Num() > 0 && SlateCount < 12)
		{
			const UWidget* W = Stack.Pop();
			if (!W) continue;
			if (TSharedPtr<SWidget> S = W->GetCachedWidget())
			{
				if (S->IsHovered())
				{
					const FVector2D Sz = S->GetCachedGeometry().GetAbsoluteSize();
					SlateHits += FString::Printf(TEXT("[%s %s hovered vis=%d %.0fx%.0f] "),
						*W->GetName(), *W->GetClass()->GetName(), W->IsVisible() ? 1 : 0, Sz.X, Sz.Y);
					++SlateCount;
				}
			}
			if (const UUserWidget* UW = Cast<const UUserWidget>(W))
			{
				if (UW->WidgetTree && UW->WidgetTree->RootWidget)
					Stack.Add(UW->WidgetTree->RootWidget);
			}
			if (const UPanelWidget* Panel = Cast<const UPanelWidget>(W))
			{
				for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
					Stack.Add(Panel->GetChildAt(i));
			}
		}
	};
	Accumulate(ChatWidget);
	Accumulate(CombatLogWidget);
	Accumulate(PlayerInterfaceWidget);
	Accumulate(QuestTrackerWidget);
	Accumulate(TradeInviteWidget);
	Accumulate(PlayerContextMenu);
	Accumulate(PlayerInspectWidget);
	Accumulate(GameMenuWidget);
	Accumulate(GameMenuBarWidget);
	Accumulate(GameSettingsWidget);
	Accumulate(KeybindWidget);
	Accumulate(DeathScreenWidget);
	UE_LOG(LogGameplay, Warning, TEXT("UIHover: registry=%s slate=[%s]"), *Key, *SlateHits);
}

int32 UUIManager::GetActiveInteractionNpcId() const
{
	if (bDialogueVisible && DialogueWidget)
	{
		const int32 Id = DialogueWidget->GetCurrentSpeakerNpcId();
		if (Id > 0) return Id;
	}
	if (bVendorShopVisible && VendorShopWidget)
	{
		const int32 Id = VendorShopWidget->GetActiveNpcId();
		if (Id > 0) return Id;
	}
	if (bRepairShopVisible && RepairShopWidget)
	{
		const int32 Id = RepairShopWidget->GetActiveNpcId();
		if (Id > 0) return Id;
	}
	if (bSkillShopVisible && SkillShopWidget)
	{
		const int32 Id = SkillShopWidget->GetActiveNpcId();
		if (Id > 0) return Id;
	}
	return 0;
}

void UUIManager::ForceCloseAllNPCWindows(UDialogueManager* DlgMgr)
{
	// Close dialogue: send server packet then hide widget immediately.
	if (bDialogueVisible && DialogueWidget)
	{
		if (DlgMgr && DlgMgr->IsDialogueActive())
			DlgMgr->CloseDialogue();
		DialogueWidget->HideDialogue();
	}
	// Close shops (each CloseShop also handles the NPC farewell-window counter).
	if (bVendorShopVisible && VendorShopWidget)
		VendorShopWidget->CloseShop();
	if (bRepairShopVisible && RepairShopWidget)
		RepairShopWidget->CloseShop();
	if (bSkillShopVisible && SkillShopWidget)
		SkillShopWidget->CloseShop();
}

bool UUIManager::ShouldShowCursor() const
{
	// Show cursor if any UI element is open or alt-cursor is active
	bool bAnyWidgetVisible = bInventoryVisible || bSkillsPanelVisible || bHarvestLootVisible 
		|| bDialogueVisible || bQuestJournalVisible
		|| bVendorShopVisible || bRepairShopVisible || bSkillShopVisible || bTradeVisible || bEquipmentVisible
		|| bPlayerStatsVisible || bBestiaryVisible || bTitlesVisible || bReputationVisible || bEmoteListVisible
		|| bAltCursorActive || bGameMenuVisible || bDeathScreenVisible;
	
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager: Cursor check -> Show: %s"),
		bAnyWidgetVisible ? TEXT("YES") : TEXT("NO"));
	
	return bAnyWidgetVisible;
}

void UUIManager::PlayCombatCameraShake(float Intensity)
{
	if (!GetWorld()) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;

	// Prefer the Blueprint-assigned class; fall back to the C++ default.
	TSubclassOf<UCameraShakeBase> ShakeClass = CombatCameraShakeClass
		? CombatCameraShakeClass
		: TSubclassOf<UCameraShakeBase>(UCombatCameraShake::StaticClass());

	PC->PlayerCameraManager->StartCameraShake(ShakeClass, Intensity);
}

void UUIManager::ShowMobTargetFrame(const FString& MobSlug, const FString& MobName, int32 MobLevel, int32 CurrentHP, int32 MaxHP, bool bIsAggro, UTexture2D* MobIcon)
{
	if (PlayerInterfaceWidget)
	{
		UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget();
		if (TargetFrame)
		{
			TargetFrame->SetMobInfo(MobSlug, MobName, MobLevel, CurrentHP, MaxHP, bIsAggro, MobIcon);
		}
	}
}

void UUIManager::HideMobTargetFrame()
{
	if (PlayerInterfaceWidget)
	{
		UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget();
		if (TargetFrame)
		{
			TargetFrame->ClearTarget();
		}
	}
}

void UUIManager::ToggleGameMenu()
{
	UE_LOG(LogGameplay, Verbose, TEXT("UIManager::ToggleGameMenu - GameMenuWidget: %s, IsMenuOpen: %s"),
		GameMenuWidget ? TEXT("Valid") : TEXT("NULL"),
		(GameMenuWidget && GameMenuWidget->IsMenuOpen()) ? TEXT("true") : TEXT("false"));

	// Toggle the game menu only — do not close other windows.
	if (GameMenuWidget)
	{
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
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("UIManager::ToggleGameMenu - GameMenuWidget is NULL!"));
	}
}

