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

void ABasicPlayer::StopAutoAttack()
{
    // Interrupt the attack cycle and any approach movement but keep LockedTarget
    // so the player can resume attacking after repositioning.
    bIsAutoAttacking = false;
    bIsApproachingTarget = false;
    PendingSkillSlug.Empty();

    GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);

    UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Auto-attack stopped (target lock retained)"));
}

void ABasicPlayer::FaceLockedTarget()
{
    AActor* TargetActor = GetLockedTargetActor();
    if (!TargetActor) return;

    FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
    ToTarget.Z = 0.f;
    if (ToTarget.IsNearlyZero()) return;

    DesiredMeshYaw    = ToTarget.Rotation().Yaw;
    bHasDesiredMeshYaw = true;
}

void ABasicPlayer::UpdateApproach(float DeltaTime)
{
    if (LockedTarget || LockedPlayerTarget)
    {
        AActor* Target = GetLockedTargetActor();
        if (IsValid(Target))
        {
            const float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
            if (Dist > 20000.0f)
            {
                ClearLockedTarget();
                bIsApproachingTarget = false;
                PendingSkillSlug.Empty();
                return;
            }
        }
    }

    if (!bIsApproachingTarget) return;

    // в”Ђв”Ђ Non-combat pending approach (NPC / Item / Harvest) в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    // These don't use LockedTarget; they use PendingInteractionTarget instead.
    if (PendingInteraction != EPendingInteraction::None)
    {
        AActor* PendingTarget = PendingInteractionTarget.Get();
        if (!IsValid(PendingTarget))
        {
            // Target disappeared вЂ” abort
            bIsApproachingTarget = false;
            PendingInteraction       = EPendingInteraction::None;
            PendingInteractionTarget = nullptr;
            return;
        }

        FVector ToPending = PendingTarget->GetActorLocation() - GetActorLocation();
        ToPending.Z = 0.f;
        float PendingDist = ToPending.Size();

        // Subtract capsule radius so we stop at the surface, not the pivot.
        if (const ACharacter* TargetChar = Cast<ACharacter>(PendingTarget))
        {
            if (const UCapsuleComponent* Cap = TargetChar->GetCapsuleComponent())
            {
                PendingDist = FMath::Max(0.f, PendingDist - Cap->GetScaledCapsuleRadius());
            }
        }

        const FVector PendingDir = ToPending.GetSafeNormal();

        AddMovementInput(PendingDir, 1.0f);
        DesiredMeshYaw    = PendingDir.Rotation().Yaw;
        bHasDesiredMeshYaw = true;

        {
            float EffectiveRange = GetInteractionRange();
            if (PendingInteraction == EPendingInteraction::Harvest)
            {
                if (UHarvestManager* HMgr = MyGameInstance ? MyGameInstance->GetHarvestManager() : nullptr)
                {
                    EffectiveRange = HMgr->GetMaxHarvestDistance();
                }
            }
            else if (PendingInteraction == EPendingInteraction::PickupItem)
            {
                EffectiveRange = CursorInteractionComponent ? CursorInteractionComponent->GetItemPickupRange() : 180.f;
            }
            if (PendingDist <= EffectiveRange)
            {
                bIsApproachingTarget = false;
                DispatchPendingInteraction();
            }
        }
        return;
    }

    // -- Combat approach (uses LockedTarget or LockedPlayerTarget) --
    AActor* CombatTarget = GetLockedTargetActor();
    if (!IsValid(CombatTarget))
    {
        bIsApproachingTarget = false;
        PendingSkillSlug.Empty();
        ClearLockedTarget();
        return;
    }

    // Mob died while approaching — stop movement but keep lock for harvesting.
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

    // Player target died while approaching
    if (LockedPlayerTarget && LockedPlayerTarget->playerData.characterData.bIsDead)
    {
        bIsApproachingTarget = false;
        PendingSkillSlug.Empty();
        return;
    }

    // Move toward the target every frame using CharacterMovementComponent
    FVector ToTarget = CombatTarget->GetActorLocation() - GetActorLocation();
    ToTarget.Z = 0.f;
    const float Dist = ToTarget.Size();
    const FVector Direction = ToTarget.GetSafeNormal();

    AddMovementInput(Direction, 1.0f);
    DesiredMeshYaw = Direction.Rotation().Yaw;
    bHasDesiredMeshYaw = true;

    // Check if we have arrived
    const bool bIsPendingSkill = !PendingSkillSlug.IsEmpty();
    const float EffectiveRange = bIsPendingSkill ? GetSkillRange(PendingSkillSlug) : GetCurrentSkillRange();

    if (Dist <= EffectiveRange)
    {
        bIsApproachingTarget = false;

        if (bIsPendingSkill)
        {
            UE_LOG(LogTemp, Log, TEXT("BasicPlayer: UpdateApproach - reached range (%.0f <= %.0f), casting %s"),
                Dist, EffectiveRange, *PendingSkillSlug);

            FaceLockedTarget();
            const FString SkillToCast = PendingSkillSlug;
            PendingSkillSlug.Empty();

            if (SkillToCast == CurrentSkillName)
            {
                // Auto-attack skill: start the repeating loop
                bIsAutoAttacking = true;
                DoAutoAttack();
            }
            else
            {
                // Targeted skill: single cast
                const int32 SlotIndex = GetSkillSlotIndexForSlug(SkillToCast);
                if (SlotIndex >= 0 && UIManager && UIManager->GetSkillBarWidget())
                {
                    UIManager->GetSkillBarWidget()->CastSkillFromSlot(SlotIndex);
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("BasicPlayer: UpdateApproach - reached target (%.0f <= %.0f), attacking"),
                Dist, EffectiveRange);
            DoAutoAttack();
        }
    }
}
