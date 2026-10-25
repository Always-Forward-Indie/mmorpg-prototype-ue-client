// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "Prototyping.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/PlayerManager.h"
#include "UI/UIManager.h"
#include "UI/NotificationToastWidget.h"
#include "UI/ChatWidget.h"
#include "UI/SkillBarWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/ItemStruct.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "UI/PlayerStatsWidget.h"
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Utils/PlayerAttributeParser.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Gameplay/UI/ActiveEffectsWidget.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/PlayerHUD.h"
#include "Gameplay/UI/CastBarWidget.h"
#include "Gameplay/Combat/BaseMMOProjectile.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Player/TitleManager.h"
#include "Data/EffectDefinitionTable.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "Data/CharacterVisualData.h"
#include "Engine/AssetManager.h"

void ABasicPlayer::TargetSelf()
{
    // Clear any external target locks
    if (LockedTarget && LockedTarget->MobHeadInfo)
    {
        LockedTarget->MobHeadInfo->ShowWidget(false);
    }
    LockedTarget = nullptr;
    LockedPlayerTarget = nullptr;
    bIsAutoAttacking = false;
    bIsApproachingTarget = false;
    PendingSkillSlug.Empty();
    GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);

    bIsSelfTargeted = true;

    if (UIManager)
    {
        UIManager->SetSkillTarget(playerData.characterData.characterId, ECasterType::Self);
        UIManager->ShowMobTargetFrame(
            TEXT(""),
            playerData.characterData.characterName,
            playerData.characterData.characterLevel,
            playerData.characterData.characterCurrentHealth,
            GetMaxHealth_Implementation(),
            false,
            nullptr);
    }

    if (CursorInteractionComponent)
    {
        CursorInteractionComponent->NotifyLockedTargetChanged(nullptr, nullptr, EInteractableType::None);
    }

    UE_LOG(LogGameplay, Verbose, TEXT("BasicPlayer: Self-targeted (%s, id=%d)"),
        *playerData.characterData.characterName, playerData.characterData.characterId);
}

void ABasicPlayer::OnTargetSelfInput()
{
    if (playerData.isOtherClient || playerData.characterData.bIsDead) return;

    if (bIsSelfTargeted)
    {
        // Toggle off: clear self target
        bIsSelfTargeted = false;
        if (UIManager)
        {
            UIManager->SetSkillTarget(0, ECasterType::None);
            UIManager->HideMobTargetFrame();
        }
        UE_LOG(LogGameplay, Verbose, TEXT("BasicPlayer: Self-target cleared"));
    }
    else
    {
        TargetSelf();
    }
}

