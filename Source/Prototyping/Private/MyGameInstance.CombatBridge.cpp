// Fill out your copyright notice in the Description page of Project Settings.
#include "MyGameInstance.h"
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


void UMyGameInstance::PlayCombatAnimation(const FCombatAnimationData& AnimationData)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Playing combat animation %s for character ID %d"),
		*AnimationData.AnimationName, AnimationData.CharacterId);

	// Check if the character is the player
	ABasicPlayer* SourcePlayer = GetPlayerByCharacterId(AnimationData.CharacterId);
	
	if (SourcePlayer)
	{
		// Play animation on the player character
		UE_LOG(LogTemp, Warning, TEXT("Playing animation on player character: %s"), *AnimationData.AnimationName);
		// TODO: Implement animation playing in BasicPlayer class
		// SourcePlayer->PlayAnimation(AnimationData.AnimationName);
	}
	else
	{
		// Check if it's a MOB
		// Convert the character ID to a string to match how MOB UIDs are stored
		FString MobUid = FString::FromInt(AnimationData.CharacterId);
		
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName(*MobUid), FoundActors);
		
		if (FoundActors.Num() > 0)
		{
			ABasicMOB* MOB = Cast<ABasicMOB>(FoundActors[0]);
			if (MOB)
			{
				// Play animation on the MOB
				UE_LOG(LogTemp, Warning, TEXT("Playing animation on MOB %s: %s"), 
					*MOB->GetMobName(), *AnimationData.AnimationName);
				// TODO: Implement animation playing in BasicMOB class
				// MOB->PlayAnimation(AnimationData.AnimationName);
			}
		}
	}
}

void UMyGameInstance::ProcessCombatAction(const FCombatActionData& ActionData)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Processing combat action %s from caster %d to target %d"),
		*ActionData.ActionName, ActionData.CasterId, ActionData.TargetId);

	// Find the source and target based on their IDs
	ABasicPlayer* SourcePlayer = GetPlayerByCharacterId(ActionData.CasterId);
	
	if (SourcePlayer)
	{
		// Handle action on player character (e.g., play attack animation)
		UE_LOG(LogTemp, Warning, TEXT("Player %d performing action %s"), 
			ActionData.CasterId, *ActionData.ActionName);
		// TODO: Implement action performing in BasicPlayer class
		// SourcePlayer->PerformAction(ActionData.ActionName);
	}
	
	// Note: The target's response to the action (e.g., taking damage) will be handled 
	// through the combatResult event and the respective update health methods
}

void UMyGameInstance::UpdateMobHealth(int32 TargetId, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Updating mob health for ID %d: Health=%d, IsDead=%s, Damage=%d"),
		TargetId, NewHealth, bIsDead ? TEXT("True") : TEXT("False"), DamageDealt);

	// Use new combat system to find and update MOB
	if (UCombatSystemManager* CombatManager = GetCombatSystemManager())
	{
		TScriptInterface<ICombatable> MobCombatable = CombatManager->FindCombatableById(TargetId, ECasterType::Mob);
		if (MobCombatable.GetInterface() && MobCombatable.GetObject() && IsValid(MobCombatable.GetObject()))
		{
			ICombatable* MobInterface = MobCombatable.GetInterface();
			
			// Update through combat system
			MobInterface->SetCurrentHealth_Implementation(NewHealth);
			MobInterface->SetCurrentMana_Implementation(NewMana);
			
			if (bIsDead)
			{
				MobInterface->SetDead_Implementation(true);
			}
			
			// Show damage effect if damaged
			//if (bIsDamaged && DamageDealt > 0)
			//{
			//	MobInterface->ShowDamageEffect_Implementation(DamageDealt, false, ESkillSchool::Physical);
			//}
			
			UE_LOG(LogTemp, Warning, TEXT("Updated MOB through combat system"));
			return;
		}
	}

}

void UMyGameInstance::UpdatePlayerHealth(int32 TargetId, int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Updating player health for ID %d: Health=%d, IsDead=%s, Damage=%d"),
		TargetId, NewHealth, bIsDead ? TEXT("True") : TEXT("False"), DamageDealt);

	// Use new combat system to find and update Player
	if (UCombatSystemManager* CombatManager = GetCombatSystemManager())
	{
		TScriptInterface<ICombatable> PlayerCombatable = CombatManager->FindCombatableById(TargetId, ECasterType::Player);
		if (PlayerCombatable.GetInterface() && PlayerCombatable.GetObject() && IsValid(PlayerCombatable.GetObject()))
		{
			ICombatable* PlayerInterface = PlayerCombatable.GetInterface();
			
			// Update through combat system
			PlayerInterface->SetCurrentHealth_Implementation(NewHealth);
			PlayerInterface->SetCurrentMana_Implementation(NewMana);
			
			// Show damage effect if damaged
			//if (bIsDamaged && DamageDealt > 0)
			//{
			//	PlayerInterface->ShowDamageEffect_Implementation(DamageDealt, false, ESkillSchool::Physical);
			//}
			
			// Handle player death if needed
			if (bIsDead)
			{
				PlayerInterface->SetDead_Implementation(true);
			}
			
			UE_LOG(LogTemp, Warning, TEXT("Updated Player through combat system"));
			return;
		}
	}
}

void UMyGameInstance::UpdateTargetHealth(int32 TargetId, int32 TargetType, const FString& TargetTypeString, 
	int32 NewHealth, int32 NewMana, bool bIsDead, bool bIsDamaged, int32 DamageDealt)
{
	UE_LOG(LogTemp, Warning, TEXT("MyGameInstance: Updating target health for %s ID %d: Health=%d, IsDead=%s, Damage=%d"),
		*TargetTypeString, TargetId, NewHealth, bIsDead ? TEXT("True") : TEXT("False"), DamageDealt);

	// Route the update based on target type
	if (TargetTypeString.Equals("mob", ESearchCase::IgnoreCase) || 
        TargetTypeString.Equals("MOB", ESearchCase::IgnoreCase) || 
        TargetType == 1 || 
        TargetType == 3)  // Support both numerical and string type identifiers
	{
		UpdateMobHealth(TargetId, NewHealth, NewMana, bIsDead, bIsDamaged, DamageDealt);
	}
	else if (TargetTypeString.Equals("player", ESearchCase::IgnoreCase) || 
             TargetTypeString.Equals("PLAYER", ESearchCase::IgnoreCase) || 
             TargetType == 2)
	{
		UpdatePlayerHealth(TargetId, NewHealth, NewMana, bIsDead, bIsDamaged, DamageDealt);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown target type: %s (Type %d) for target %d"), 
			*TargetTypeString, TargetType, TargetId);
	}
}
