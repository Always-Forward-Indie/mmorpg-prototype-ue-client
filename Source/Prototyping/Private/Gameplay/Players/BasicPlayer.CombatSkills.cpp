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

int32 ABasicPlayer::GetSkillSlotIndexForSlug(const FString& SkillSlug) const
{
    if (!MyGameInstance) return -1;
    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    if (!SkillMgr) return -1;

    TArray<FSkillSlotData> AllSlots = SkillMgr->GetAllSkillSlots();
    for (const FSkillSlotData& Slot : AllSlots)
    {
        if (Slot.bIsAssigned && Slot.skillSlug == SkillSlug)
        {
            return Slot.slotIndex;
        }
    }
    return -1;
}

float ABasicPlayer::GetSkillRange(const FString& SkillSlug) const
{
    if (MyGameInstance)
    {
        if (UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager())
        {
            if (SkillMgr->HasSkill(SkillSlug))
            {
                const float ServerRange = SkillMgr->GetSkillData(SkillSlug).networkData.maxRange;
                if (ServerRange > 0.0f)
                {
                    return ServerRange * 100.0f - AttackRangeServerTolerance;
                }
            }
        }
    }
    return AttackRange - AttackRangeServerTolerance;
}

float ABasicPlayer::GetCurrentSkillRange() const
{
    // Try to read maxRange from the server-initialized skill data.
    // The server stores range in "game units" and compares as: distance > maxRange * 100.0f
    // so we apply the same multiplier here for an identical range check on the client.
    if (MyGameInstance)
    {
        if (UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager())
        {
            if (SkillMgr->HasSkill(CurrentSkillName))
            {
                const float ServerRange = SkillMgr->GetSkillData(CurrentSkillName).networkData.maxRange;
                if (ServerRange > 0.0f)
                {
                    return ServerRange * 100.0f - AttackRangeServerTolerance;
                }
            }
        }
    }

    // Fallback: use the Blueprint-editable AttackRange with tolerance applied.
    return AttackRange - AttackRangeServerTolerance;
}

void ABasicPlayer::TryCastSkillWithApproach(const FString& SkillSlug)
{
    if (playerData.characterData.bIsDead) return;

    // Need a locked target (mob or player) to approach/cast
    AActor* TargetActor = GetLockedTargetActor();
    if (!TargetActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: TryCastSkillWithApproach - no locked target"));
        return;
    }

    // Mob-specific: dead mob handling
    if (IsValid(LockedTarget) && LockedTarget->GetMOBIsDead())
    {
        const bool bWasNonCombat = (PendingInteraction == EPendingInteraction::Harvest ||
                                    PendingInteraction == EPendingInteraction::TalkNPC   ||
                                    PendingInteraction == EPendingInteraction::PickupItem);
        if (!bWasNonCombat)
        {
            bIsApproachingTarget = false;
            PendingInteraction   = EPendingInteraction::None;
        }
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(LockedTarget, EInteractableType::MOB_Harvestable);
        return;
    }

    // Player-specific: dead player handling
    if (LockedPlayerTarget && LockedPlayerTarget->playerData.characterData.bIsDead)
    {
        bIsApproachingTarget = false;
        PendingSkillSlug.Empty();
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: TryCastSkillWithApproach - player target is dead"));
        return;
    }

    const float Dist = FVector::Dist(GetActorLocation(), TargetActor->GetActorLocation());
    const float EffectiveRange = GetSkillRange(SkillSlug);

    UE_LOG(LogTemp, Log, TEXT("BasicPlayer: TryCastSkillWithApproach - skill=%s dist=%.0f range=%.0f"),
        *SkillSlug, Dist, EffectiveRange);

    if (Dist <= EffectiveRange)
    {
        // Already in range
        FaceLockedTarget();
        if (SkillSlug == CurrentSkillName)
        {
            // Auto-attack skill: start the repeating attack loop
            bIsAutoAttacking = true;
            DoAutoAttack();
        }
        else
        {
            // Targeted skill: single cast via skill bar
            if (UIManager && UIManager->GetSkillBarWidget())
            {
                UIManager->GetSkillBarWidget()->CastSkillFromSlot(
                    GetSkillSlotIndexForSlug(SkillSlug));
            }
        }
    }
    else
    {
        // Out of range — store pending skill and start approach.
        // Clear any stale non-combat PendingInteraction so UpdateApproach
        // takes the combat path and moves toward LockedTarget instead of
        // a previous PendingInteractionTarget.
        PendingInteraction       = EPendingInteraction::None;
        PendingInteractionTarget = nullptr;
        PendingSkillSlug = SkillSlug;
        // For the auto-attack skill keep bIsAutoAttacking true so DoAutoAttack
        // can continue the loop after the first swing.
        bIsAutoAttacking  = (SkillSlug == CurrentSkillName);
        bIsApproachingTarget = true;

        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: TryCastSkillWithApproach - out of range (%.0f > %.0f), approaching for skill %s"),
            Dist, EffectiveRange, *SkillSlug);
    }
}

void ABasicPlayer::DoAutoAttack()
{
    if (playerData.characterData.bIsDead || !bIsAutoAttacking) return;

    if (!IsValid(LockedTarget))
    {
        ClearLockedTarget();
        return;
    }

    if (LockedTarget->GetMOBIsDead())
    {
        bIsAutoAttacking    = false;
        bIsApproachingTarget = false;
        PendingSkillSlug.Empty();
        GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(LockedTarget, EInteractableType::MOB_Harvestable);
        return;
    }

    const float Dist = FVector::Dist(GetActorLocation(), LockedTarget->GetActorLocation());
    const float EffectiveRange = GetCurrentSkillRange();

    if (Dist > EffectiveRange)
    {
        if (!bIsApproachingTarget) bIsApproachingTarget = true;
        return;
    }

    if (bIsApproachingTarget) bIsApproachingTarget = false;

    FaceLockedTarget();

    const int32 MobId = FCString::Atoi(*LockedTarget->GetMOBUId());

    const bool bSent = AttackTarget(MobId, CurrentSkillName, 3);

    const float RetryDelay = bSent ? 5.0f : 0.15f;
    GetWorld()->GetTimerManager().SetTimer(
        AutoAttackRetryTimerHandle, this, &ABasicPlayer::DoAutoAttack, RetryDelay, false);
}

void ABasicPlayer::HandleSkillInitiatedForAutoAttack(const FString& SkillSlug, int32 CooldownMs)
{
    if (!bIsAutoAttacking) return;
    if (!SkillSlug.Equals(CurrentSkillName, ESearchCase::IgnoreCase)) return;

    GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);

    if (!IsValid(LockedTarget) || LockedTarget->GetMOBIsDead())
    {
        StopAutoAttack();
        return;
    }

    const float Delay = (CooldownMs > 0)
        ? (static_cast<float>(CooldownMs) / 1000.0f + AutoAttackSwingDelay)
        : AutoAttackSwingDelay;

    GetWorld()->GetTimerManager().SetTimer(
        AutoAttackRetryTimerHandle, this, &ABasicPlayer::DoAutoAttack, Delay, false);
}


bool ABasicPlayer::AttackTarget(int32 TargetID, const FString& SkillSlug, int32 TargetTypeId)
{
	if (!MyGameInstance || TargetID <= 0) return false;

	const ECasterType TargetType = (TargetTypeId == 2) ? ECasterType::Player : ECasterType::Mob;

	// Route through PlayerSkillManager::TryCastSkill so every guard (GCD, CD,
	// animation lock, server-confirmation round-trip) is active.  This single
	// call replaces the old SkillSystemManager::CanCastSkill + CastSkill path
	// which had no per-player gating and allowed duplicate attack requests.
	if (UPlayerSkillManager* PSM = MyGameInstance->GetPlayerSkillManager())
	{
		if (PSM->TryCastSkill(SkillSlug, TargetID, TargetType))
		{
			UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: %d attacking target %d with %s"),
				GetActorId_Implementation(), TargetID, *SkillSlug);
			return true;
		}
	}
	return false;
}

