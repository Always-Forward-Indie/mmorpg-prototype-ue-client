// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
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

void ABasicPlayer::SetLockedTarget(ABasicMOB* NewTarget)
{
    if (LockedTarget == NewTarget) return;

    // Any previous non-combat pending interaction (e.g. double-click approach
    // toward a different target) is now stale — clear it so UpdateApproach
    // uses the combat path and moves toward the new LockedTarget.
    PendingInteraction       = EPendingInteraction::None;
    PendingInteractionTarget = nullptr;

    // Clear any player/self target when locking a mob
    if (NewTarget && LockedPlayerTarget)
    {
        LockedPlayerTarget = nullptr;
    }
    if (NewTarget) bIsSelfTargeted = false;

    // Hide head info of the old locked target before switching
    if (LockedTarget && LockedTarget->MobHeadInfo)
    {
        LockedTarget->MobHeadInfo->ShowWidget(false);
    }

    LockedTarget = NewTarget;

    if (LockedTarget)
    {
        const int32 MobId = FCString::Atoi(*LockedTarget->GetMOBUId());
        if (UIManager)
        {
            UIManager->SetSkillTarget(MobId, ECasterType::Mob);

            bool bIsAggro = LockedTarget->GetMOBIsAggressive();

            UIManager->ShowMobTargetFrame(
                LockedTarget->GetMOBData().mobSlug,
                LockedTarget->GetMobName(),
                LockedTarget->GetMOBLevel(),
                LockedTarget->GetMOBCurrentHealth(),
                LockedTarget->GetMOBAttributes().attributesData.Contains(TEXT("max_health"))
                    ? LockedTarget->GetMOBAttributes().attributesData[TEXT("max_health")].attributeValue
                    : 100,
                bIsAggro,
                LockedTarget->CachedIcon);
            bLastKnownTargetAggro = bIsAggro;
        }

        if (LockedTarget->MobHeadInfo)
        {
            LockedTarget->MobHeadInfo->ShowWidget(true);
        }

        FaceLockedTarget();
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Locked target -> MOB %d (%s)"),
            MobId, *LockedTarget->GetMobName());
    }

    if (CursorInteractionComponent)
    {
        const EInteractableType NewType = NewTarget ? NewTarget->GetInteractableType() : EInteractableType::None;
        CursorInteractionComponent->NotifyLockedTargetChanged(nullptr, NewTarget, NewType);
    }
}

void ABasicPlayer::ClearLockedTarget()
{
    if (LockedTarget)
    {
        // Stop keeping the head info widget forced visible
        if (LockedTarget->MobHeadInfo)
        {
            LockedTarget->MobHeadInfo->ShowWidget(false);
        }
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Cleared locked target"));
    }

    LockedTarget = nullptr;
    LockedPlayerTarget = nullptr;
    bIsSelfTargeted = false;
    PrevSoftTarget = nullptr;
    bIsAutoAttacking = false;
    bIsApproachingTarget = false;
    PendingSkillSlug.Empty();

    GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);

    if (UIManager)
    {
        UIManager->SetSkillTarget(0, ECasterType::None);
        UIManager->HideMobTargetFrame();
    }

    // Notify cursor system so decal is released
    if (CursorInteractionComponent)
    {
        CursorInteractionComponent->NotifyLockedTargetChanged(nullptr, nullptr, EInteractableType::None);
    }
}

void ABasicPlayer::SetLockedPlayerTarget(ABasicPlayer* NewTarget)
{
    if (LockedPlayerTarget == NewTarget) return;
    if (NewTarget == this) return; // Cannot target yourself via click

    // Clear mob target when locking a player
    if (NewTarget && LockedTarget)
    {
        if (LockedTarget->MobHeadInfo)
        {
            LockedTarget->MobHeadInfo->ShowWidget(false);
        }
        LockedTarget = nullptr;
        bIsAutoAttacking = false;
        bIsApproachingTarget = false;
        PendingSkillSlug.Empty();
        GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);
    }

    LockedPlayerTarget = NewTarget;

    if (LockedPlayerTarget)
    {
        bIsSelfTargeted = false;
        const int32 PlayerId = LockedPlayerTarget->playerData.characterData.characterId;
        if (UIManager)
        {
            UIManager->SetSkillTarget(PlayerId, ECasterType::Player);

            // Show the target frame with player info (reuse mob target frame)
            UIManager->ShowMobTargetFrame(
                TEXT(""), // no mob slug
                LockedPlayerTarget->playerData.characterData.characterName,
                LockedPlayerTarget->playerData.characterData.characterLevel,
                LockedPlayerTarget->playerData.characterData.characterCurrentHealth,
                LockedPlayerTarget->GetMaxHealth_Implementation(),
                false, // players are not aggressive
                nullptr); // no icon
        }

        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Locked player target -> Player %d (%s)"),
            PlayerId, *LockedPlayerTarget->playerData.characterData.characterName);
    }

    if (CursorInteractionComponent)
    {
        CursorInteractionComponent->NotifyLockedTargetChanged(nullptr, NewTarget, 
            NewTarget ? EInteractableType::RemotePlayer : EInteractableType::None);
    }
}

AActor* ABasicPlayer::GetLockedTargetActor() const
{
    if (LockedTarget) return LockedTarget;
    if (LockedPlayerTarget) return LockedPlayerTarget;
    return nullptr;
}

int32 ABasicPlayer::GetLockedTargetId() const
{
    if (LockedTarget)
    {
        return FCString::Atoi(*LockedTarget->GetMOBUId());
    }
    if (LockedPlayerTarget)
    {
        return LockedPlayerTarget->playerData.characterData.characterId;
    }
    return 0;
}

ECasterType ABasicPlayer::GetLockedTargetType() const
{
    if (LockedTarget) return ECasterType::Mob;
    if (LockedPlayerTarget) return ECasterType::Player;
    return ECasterType::None;
}

