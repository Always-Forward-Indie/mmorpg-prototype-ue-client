// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
#include "Prototyping.h"
#include "Components/ArrowComponent.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "DevMode/DevModeDataProvider.h"
#include "DevMode/DevModeConsoleCommands.h"
#include "DevMode/LiveBotDriver.h"
#include "Containers/Ticker.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Combat/DamageEffectHandler.h"
#include "Gameplay/Combat/HealingEffectHandler.h"
#include "Gameplay/Combat/BuffEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/ExperienceNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/MasteryManager.h"
#include "Gameplay/Player/MasteryNetworkHandler.h"
#include "Gameplay/Player/ReputationManager.h"
#include "Gameplay/Player/ReputationNetworkHandler.h"
#include "Gameplay/Player/TitleManager.h"
#include "Gameplay/Player/TitleNetworkHandler.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Gameplay/NPCs/AmbientSpeechNetworkHandler.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Gameplay/WorldObjects/WIONetworkHandler.h"
#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Animation/AnimNotify_Footstep.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Gameplay/Skills/PlayerSkillSystemFactory.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "Services/TimeSyncService.h"
#include "Networking/PingManager.h"
#include "UI/UIManager.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/NPCs/NPCNetworkHandler.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Dialogue/DialogueNetworkHandler.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Quest/QuestNetworkHandler.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Trade/TradeNetworkHandler.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentNetworkHandler.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Vendor/VendorNetworkHandler.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Repair/RepairNetworkHandler.h"
#include "Gameplay/SkillShop/SkillShopManager.h"
#include "Gameplay/SkillShop/SkillShopNetworkHandler.h"
#include "Gameplay/Characters/CharacterPreviewManager.h"
#include "Gameplay/LoginLevel/LoginLevelSetupActor.h"

#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Chat/ChatNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/ICursor.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "Engine/GameViewportClient.h"
#include "Camera/CameraComponent.h"
#include "UObject/UObjectGlobals.h"
#include "Audio/MusicZoneActor.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"


void UMyGameInstance::MaybeStartLiveBot()
{
	if (LiveBotScenario.IsEmpty()) { return; }
	if (DevModeConfig.bEnabled)
	{
		UE_LOG(LogGameplay, Warning, TEXT("LiveBot: -bot-live ignored (DevMode is enabled — live runs need the online flow)"));
		return;
	}
	if (!LiveBotScenario.Equals(TEXT("live_patrol_kill"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_cross_visibility"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_loot_pickup"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_death_respawn"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_harvest_run"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_vendor_repair"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_vendor_negative"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_quest_turnin"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_skills"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_equip"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_level"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_titles_emotes"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_trade"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_char_create_delete"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_back_to_login"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_travel_roundtrip"), ESearchCase::IgnoreCase)
		&& !LiveBotScenario.Equals(TEXT("live_reconnect"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogGameplay, Warning, TEXT("LiveBot: unknown scenario '%s' (use live_patrol_kill|live_cross_visibility|live_loot_pickup|live_death_respawn|live_harvest_run|live_vendor_repair|live_vendor_negative|live_quest_turnin|live_skills|live_equip|live_level|live_titles_emotes|live_char_create_delete|live_back_to_login|live_travel_roundtrip|live_reconnect|live_trade)"), *LiveBotScenario);
		return;
	}
	ULiveBotDriver* Driver = LiveBotDriver;
	if (Driver && Driver->IsRunning())
	{
		UE_LOG(LogGameplay, Warning, TEXT("LiveBot: driver already running"));
		return;
	}
	if (!Driver)
	{
		Driver = NewObject<ULiveBotDriver>(this);
		LiveBotDriver = Driver;
	}
	Driver->Start(this, LiveBotScenario);
}


void UMyGameInstance::RunDevModeAutoScenario()
{
	if (DevModeAutoScenario.IsEmpty() || !DevModeConsoleCommands)
	{
		return;
	}
	UE_LOG(LogGameplay, Warning, TEXT("GameInstance: running DevMode auto-scenario '%s'"), *DevModeAutoScenario);
	bDevModeAutoQuit = true;
	DevModeConsoleCommands->RunScenario(DevModeAutoScenario);
}

void UMyGameInstance::SweepStep2()
{
	if (DevModeConsoleCommands)
	{
		DevModeConsoleCommands->RunSweepPhase2();
	}
}

void UMyGameInstance::SweepStep3()
{
	if (DevModeConsoleCommands)
	{
		DevModeConsoleCommands->RunSweepPhase3();
		// L4 soak loop: more cycles pending — schedule the next one and
		// stay alive instead of auto-quitting below.
		if (DevModeConsoleCommands->SoakContinue())
		{
			DevModeConsoleCommands->ScheduleSoakCycle();
			return;
		}
	}
	if (bDevModeAutoQuit)
	{
		bDevModeAutoQuit = false;
		DevModeAutoScenario.Empty();
		// Phase-2 tombstone is in the log; quit so CI gates on log scan.
		if (UWorld* World = GetWorld())
		{
			if (GEngine)
			{
				GEngine->Exec(World, TEXT("quit"));
			}
		}
	}
}

void UMyGameInstance::ScheduleSweepPhase2(float DelaySec)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DevModeSweepTimerHandle, this,
			&UMyGameInstance::SweepStep2, DelaySec, false);
	}
}

void UMyGameInstance::ScheduleSweepPhase3(float DelaySec)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DevModeSweepTimerHandle, this,
			&UMyGameInstance::SweepStep3, DelaySec, false);
	}
}

void UMyGameInstance::ScheduleSoakCycle(float DelaySec)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DevModeSoakTimerHandle, this,
			&UMyGameInstance::SoakCycleStep, DelaySec, false);
	}
}

void UMyGameInstance::SoakCycleStep()
{
	if (DevModeConsoleCommands)
	{
		DevModeConsoleCommands->RunSoakCycle();
	}
}
