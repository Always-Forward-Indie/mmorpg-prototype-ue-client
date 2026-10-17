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


// Implementation of missing input methods
void ABasicPlayer::OnAttackInput()
{
    if (playerData.characterData.bIsDead) return;
    if (bIsCasting || bIsPickingUp) return;

    // Toggle off auto-attack if it is already running on the locked target.
    if (LockedTarget && bIsAutoAttacking)
    {
        StopAutoAttack();
        UE_LOG(LogTemp, Warning, TEXT("Auto-attack cancelled"));
        return;
    }

    // Only attack if the player already has a target lock (cursor click sets the lock).
    // No implicit sweep-to-find-nearest: the player must explicitly target via cursor.
    if (!LockedTarget || LockedTarget->GetMOBIsDead())
    {
        UE_LOG(LogTemp, Warning, TEXT("OnAttackInput: no live locked target"));
        return;
    }

    bIsAutoAttacking = true;
    DoAutoAttack();
}

void ABasicPlayer::OnJumpPressed()
{
    if (playerData.characterData.bIsDead) return;
    if (bIsCasting || bIsPickingUp || bMovementLocked) return;
    Jump();
}

void ABasicPlayer::OnJumpReleased()
{
    StopJumping();
}

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

    UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Self-targeted (%s, id=%d)"),
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
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Self-target cleared"));
    }
    else
    {
        TargetSelf();
    }
}

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

