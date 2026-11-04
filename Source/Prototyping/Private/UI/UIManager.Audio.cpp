// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/UIManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Components/Widget.h"
#include "UI/InventoryWidget.h"
#include "UI/AvailableSkillsWidget.h"
#include "UI/HarvestLootWidget.h"
#include "UI/DialogueWidget.h"
#include "UI/QuestJournalWidget.h"
#include "UI/QuestTrackerWidget.h"
#include "UI/VendorShopWidget.h"
#include "UI/RepairShopWidget.h"
#include "UI/SkillShopWidget.h"
#include "UI/TradeWidget.h"
#include "UI/EquipmentWidget.h"
#include "UI/PlayerStatsWidget.h"
#include "UI/BestiaryWidget.h"
#include "UI/TitlesWidget.h"
#include "UI/ReputationWidget.h"
#include "UI/EmoteListWidget.h"
#include "UI/ChatWidget.h"
#include "UI/CombatLogWidget.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "UI/TradeInviteWidget.h"
#include "UI/PlayerContextMenuWidget.h"
#include "UI/PlayerInspectWidget.h"
#include "UI/GameMenuWidget.h"
#include "UI/GameMenuBarWidget.h"
#include "UI/W_SettingsWidget.h"
#include "UI/KeybindWidget.h"
#include "Gameplay/UI/DeathScreenWidget.h"

void UUIManager::ApplyButtonSoundsToAllUI()
{
	UMyGameInstance* GI = GetWorld() ? Cast<UMyGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!GI || !GI->AudioManager)
	{
		return;
	}
	UAudioManager* Audio = GI->AudioManager;
	UWidget* Roots[] = {
		InventoryWidget, AvailableSkillsWidget, HarvestLootWidget, DialogueWidget,
		QuestJournalWidget, QuestTrackerWidget, VendorShopWidget, RepairShopWidget,
		SkillShopWidget, TradeWidget, EquipmentWidget, PlayerStatsWidget,
		BestiaryWidget, TitlesWidget, ReputationWidget, EmoteListWidget,
		ChatWidget, CombatLogWidget, PlayerInterfaceWidget, TradeInviteWidget,
		PlayerContextMenu, PlayerInspectWidget, GameMenuWidget, GameMenuBarWidget,
		GameSettingsWidget, KeybindWidget, DeathScreenWidget,
	};
	for (UWidget* Root : Roots)
	{
		Audio->ApplyButtonSoundsToSubtree(Root);
	}
}
