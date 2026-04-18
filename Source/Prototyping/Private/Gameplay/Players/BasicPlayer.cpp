// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/PlayerManager.h"
#include "UI/UIManager.h"
#include "UI/SkillBarWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
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

// Convert ESkillSchool to EDamageType for FloatingCombatTextManager
static EDamageType SchoolToDamageType(ESkillSchool School)
{
    switch (School)
    {
    case ESkillSchool::Fire:    return EDamageType::Fire;
    case ESkillSchool::Ice:     return EDamageType::Ice;
    default:                    return EDamageType::Physical;
    }
}

// Spawn a one-shot SFX with SoundClassOverride set BEFORE Play() is called.
// SpawnSoundAtLocation calls Play() internally so any class override set afterwards
// is ignored by the audio engine for that playback.
static UAudioComponent* SpawnSFXAttached(AActor* Owner, USoundBase* Sound,
    const FVector& WorldLocation, float VolumeMultiplier = 1.0f)
{
    if (!Owner || !Sound) { return nullptr; }
    USoundClass* SFXClass = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
    {
        if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
    }
    if (!SFXClass)
    {
        return UGameplayStatics::SpawnSoundAtLocation(
            Owner, Sound, WorldLocation, FRotator::ZeroRotator, VolumeMultiplier);
    }
    UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
        Sound, Owner->GetRootComponent(), NAME_None,
        WorldLocation, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
        /*bStopWhenAttachedToDestroyed=*/true,
        VolumeMultiplier, 1.0f, 0.0f, nullptr, nullptr,
        /*bAutoActivate=*/false);
    if (AC)
    {
        AC->SoundClassOverride = SFXClass;
        AC->bAutoDestroy = true;
        AC->Play();
    }
    return AC;
}

// Implementation of missing input methods
void ABasicPlayer::OnAttackInput()
{
    if (playerData.characterData.bIsDead) return;

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

void ABasicPlayer::SetLockedTarget(ABasicMOB* NewTarget)
{
    if (LockedTarget == NewTarget) return;

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
    if (!LockedTarget) return;

    FVector ToTarget = LockedTarget->GetActorLocation() - GetActorLocation();
    ToTarget.Z = 0.f;
    if (ToTarget.IsNearlyZero()) return;

    DesiredMeshYaw    = ToTarget.Rotation().Yaw;
    bHasDesiredMeshYaw = true;
}

void ABasicPlayer::UpdateApproach(float DeltaTime)
{
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

        if (PendingDist <= GetInteractionRange())
        {
            bIsApproachingTarget = false;
            DispatchPendingInteraction();
        }
        return;
    }

    // в”Ђв”Ђ Combat approach (uses LockedTarget) в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    if (!IsValid(LockedTarget))
    {
        bIsApproachingTarget = false;
        PendingSkillSlug.Empty();
        ClearLockedTarget();
        return;
    }
    if (LockedTarget->GetMOBIsDead())
    {
        // Mob died while we were approaching вЂ” stop movement but keep lock for harvesting.
        bIsApproachingTarget = false;
        PendingSkillSlug.Empty();
        StopAutoAttack();
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(LockedTarget, EInteractableType::MOB_Harvestable);
        return;
    }

    // Move toward the target every frame using CharacterMovementComponent
    FVector ToTarget = LockedTarget->GetActorLocation() - GetActorLocation();
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

    // Need a locked target to approach/cast
    if (!LockedTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: TryCastSkillWithApproach - no locked target"));
        return;
    }

    if (LockedTarget->GetMOBIsDead())
    {
        // Mob died before we cast вЂ” stop approach but keep lock for harvesting.
        bIsAutoAttacking   = false;
        bIsApproachingTarget = false;
        PendingSkillSlug.Empty();
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(LockedTarget, EInteractableType::MOB_Harvestable);
        return;
    }

    const float Dist = FVector::Dist(GetActorLocation(), LockedTarget->GetActorLocation());
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
        // Out of range пїЅ store pending skill and start approach
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

    // Validate target
    if (!IsValid(LockedTarget))
    {
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: DoAutoAttack - target invalid, clearing lock"));
        ClearLockedTarget();
        return;
    }

    if (LockedTarget->GetMOBIsDead())
    {
        // Mob died вЂ” stop the attack cycle but KEEP the target lock so the
        // player can immediately harvest the corpse without re-clicking.
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: DoAutoAttack - mob dead, keeping lock for harvest"));
        StopAutoAttack();
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(LockedTarget, EInteractableType::MOB_Harvestable);
        return;
    }

    // Range check пїЅ use the same formula the server applies: maxRange * 100.0f
    const float Dist = FVector::Dist(GetActorLocation(), LockedTarget->GetActorLocation());
    const float EffectiveRange = GetCurrentSkillRange();

    UE_LOG(LogTemp, Log, TEXT("BasicPlayer: DoAutoAttack - dist=%.0f, effectiveRange=%.0f (skill=%s)"),
        Dist, EffectiveRange, *CurrentSkillName);

    if (Dist > EffectiveRange)
    {
        if (!bIsApproachingTarget)
        {
            bIsApproachingTarget = true;
            UE_LOG(LogTemp, Log, TEXT("BasicPlayer: DoAutoAttack - out of range (%.0f > %.0f), starting approach"), Dist, EffectiveRange);
        }
        return;
    }

    // In range пїЅ make sure any lingering approach is stopped
    if (bIsApproachingTarget)
    {
        bIsApproachingTarget = false;
    }

    // Face the target before swinging
    FaceLockedTarget();

    const int32 MobId = FCString::Atoi(*LockedTarget->GetMOBUId());

    // Bind next swing to fire after this animation ends
    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        if (AutoAttackAnimEndDelegateHandle.IsValid())
        {
            AnimInst->OnAttackEnded.Remove(AutoAttackAnimEndDelegateHandle);
            AutoAttackAnimEndDelegateHandle.Reset();
        }

        AutoAttackAnimEndDelegateHandle = AnimInst->OnAttackEnded.AddLambda([this]()
        {
            // Cancel the fallback timer вЂ” the normal animation chain is alive.
            GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);

            if (bIsAutoAttacking && IsValid(LockedTarget) && !LockedTarget->GetMOBIsDead())
            {
                // Normal path: schedule the next swing after a short delay.
                GetWorld()->GetTimerManager().SetTimer(AutoAttackRetryTimerHandle,
                    this, &ABasicPlayer::DoAutoAttack, AutoAttackSwingDelay, false);
            }
            else if (IsValid(LockedTarget) && LockedTarget->GetMOBIsDead())
            {
                // Mob died during this swing. Stop attacking but keep the lock
                // so the player can harvest without re-clicking.
                StopAutoAttack();
                if (CursorInteractionComponent)
                    CursorInteractionComponent->SetVisualLock(LockedTarget, EInteractableType::MOB_Harvestable);
            }
            else if (!IsValid(LockedTarget))
            {
                // Target completely gone вЂ” release the lock.
                ClearLockedTarget();
            }
            // else: bIsAutoAttacking=false but target still alive (manual skill mid-swing) вЂ” keep lock.
        });
    }
    else
    {
        // No AnimInstance пїЅ retry by simple timer as fallback
        GetWorld()->GetTimerManager().SetTimer(AutoAttackRetryTimerHandle,
            this, &ABasicPlayer::DoAutoAttack, 1.5f, false);
    }

    AttackTarget(MobId, CurrentSkillName, 3);

    // Fallback safety timer: if the server never responds (cooldown desync, packet loss)
    // the OnAttackEnded lambda will never fire and the chain would silently die.
    // Set a generous timeout here; the lambda overwrites it with a shorter delay on success.
    GetWorld()->GetTimerManager().SetTimer(
        AutoAttackRetryTimerHandle, this, &ABasicPlayer::DoAutoAttack, 3.0f, false);
}

void ABasicPlayer::OnTabTargetInput()
{
    if (playerData.characterData.bIsDead || playerData.isOtherClient) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
    const FVector CameraForward = CameraRotation.Vector();

    const float MaxTabRange = 1500.0f;
    const FVector PlayerLoc = GetActorLocation();

    // Collect all living mobs within range пїЅ no cone filter so Tab cycles
    // through everything nearby regardless of which way the mesh faces
    TArray<ABasicMOB*> Candidates;
    for (TActorIterator<ABasicMOB> It(GetWorld()); It; ++It)
    {
        ABasicMOB* Mob = *It;
        if (!Mob || Mob->GetMOBIsDead()) continue;

        if (FVector::Dist(PlayerLoc, Mob->GetActorLocation()) > MaxTabRange) continue;

        Candidates.Add(Mob);
    }

    if (Candidates.Num() == 0)
    {
        ClearLockedTarget();
        return;
    }

    // Sort by dot product to camera forward пїЅ mobs closest to crosshair come first,
    // then by distance as a tiebreaker
    Candidates.Sort([&CameraLocation, &CameraForward](const ABasicMOB& A, const ABasicMOB& B)
    {
        const FVector ToA = (A.GetActorLocation() - CameraLocation).GetSafeNormal();
        const FVector ToB = (B.GetActorLocation() - CameraLocation).GetSafeNormal();
        const float DotA = FVector::DotProduct(CameraForward, ToA);
        const float DotB = FVector::DotProduct(CameraForward, ToB);
        if (FMath::Abs(DotA - DotB) > 0.01f)
            return DotA > DotB;
        return FVector::DistSquared(CameraLocation, A.GetActorLocation())
             < FVector::DistSquared(CameraLocation, B.GetActorLocation());
    });

    // Cycle: advance past the current lock to the next candidate
    int32 NextIndex = 0;
    if (LockedTarget)
    {
        for (int32 i = 0; i < Candidates.Num(); ++i)
        {
            if (Candidates[i] == LockedTarget)
            {
                NextIndex = (i + 1) % Candidates.Num();
                break;
            }
        }
    }

    SetLockedTarget(Candidates[NextIndex]);
}

void ABasicPlayer::LockMovementForPickup()
{
    if (bIsPickingUp) return;
    bIsPickingUp = true;

    // Stop any active approach/auto-attack so the player stands still
    StopAutoAttack();

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->DisableMovement();
    }
    UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Movement locked for pickup"));
}

void ABasicPlayer::UnlockMovementAfterPickup()
{
    if (!bIsPickingUp) return;
    bIsPickingUp = false;

    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->SetMovementMode(MOVE_Walking);
    }
    UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Movement unlocked after pickup"));
}

void ABasicPlayer::OnPickupInput()
{
    if (playerData.characterData.bIsDead) return;
    if (bIsPickingUp) return;
    UE_LOG(LogTemp, Warning, TEXT("Pickup input pressed"));

    if (!InventoryManager) return;

    // NotifyPickup() + delegate binding + server request are all inside InventoryManager::PickupNearbyItem -> ItemManager::SendPickUpItemRequest.
    // Calling NotifyPickup() here first and then delaying the request via a
    // timer caused the montage to be started twice (once here, once inside
    // SendPickUpItemRequest), which produced the double-animation bug.
    InventoryManager->PickupNearbyItem();
}

void ABasicPlayer::OnInventoryToggle()
{
    UE_LOG(LogTemp, Warning, TEXT("Inventory toggle pressed"));
    
    if (InventoryManager)
    {
        InventoryManager->ToggleInventoryUI();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Inventory manager not found"));
    }
}

void ABasicPlayer::OnHarvestInput()
{
    if (playerData.characterData.bIsDead) return;
    UE_LOG(LogTemp, Warning, TEXT("Harvest input pressed"));
    
    if (!MyGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("MyGameInstance not found"));
        return;
    }
    
    UHarvestManager* HarvestManager = MyGameInstance->GetHarvestManager();
    if (HarvestManager)
    {
        HarvestManager->TryHarvestNearbyCorpse();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Harvest manager not found"));
    }
}

void ABasicPlayer::OnSkillsPanelToggle()
{
    UE_LOG(LogTemp, Warning, TEXT("Skills panel toggle pressed"));
    
    if (UIManager)
    {
        UIManager->ToggleSkillsPanel();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("UI manager not found"));
    }
}

void ABasicPlayer::OnGameMenuToggle()
{
    // If currently channeling a WIO, Escape cancels the channel instead of opening the menu
    if (MyGameInstance)
    {
        UWorldObjectManager* WOM = MyGameInstance->GetWorldObjectManager();
        if (WOM && WOM->IsChanneling())
        {
            CancelWIOChannelIfActive();
            return;
        }
    }

    if (UIManager)
    {
        UIManager->ToggleGameMenu();
    }
}

void ABasicPlayer::OnSkill1Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(0) : TEXT("");
    if (!SkillSlug.IsEmpty() && LockedTarget)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(0);
    }
}

void ABasicPlayer::OnSkill2Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(1) : TEXT("");
    if (!SkillSlug.IsEmpty() && LockedTarget)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(1);
    }
}

void ABasicPlayer::OnSkill3Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(2) : TEXT("");
    if (!SkillSlug.IsEmpty() && LockedTarget)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(2);
    }
}

void ABasicPlayer::OnSkill4Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(3) : TEXT("");
    if (!SkillSlug.IsEmpty() && LockedTarget)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(3);
    }
}

void ABasicPlayer::OnSkill5Input()
{
    if (playerData.characterData.bIsDead) return;
    if (!MyGameInstance || !UIManager || !UIManager->GetSkillBarWidget()) return;

    UPlayerSkillManager* SkillMgr = MyGameInstance->GetPlayerSkillManager();
    const FString SkillSlug = SkillMgr ? SkillMgr->GetSkillSlugFromSlot(4) : TEXT("");
    if (!SkillSlug.IsEmpty() && LockedTarget)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(4);
    }
}

void ABasicPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (EnhancedInputComponent && InputMappingContext)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnhancedInputComponent init"));
        // Get the player controller
        APlayerController* PC = Cast<APlayerController>(GetController());

        // Get the local player subsystem
        UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
        // Clear out existing mapping, and add our mapping
        Subsystem->ClearAllMappings();
        Subsystem->AddMappingContext(InputMappingContext, 1);
 
		// Bind Enhanced Input actions
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABasicPlayer::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABasicPlayer::Look);

        // WoW-style mouse button bindings
        if (RightMouseAction)
        {
            EnhancedInputComponent->BindAction(RightMouseAction, ETriggerEvent::Started,   this, &ABasicPlayer::OnRightMousePressed);
            EnhancedInputComponent->BindAction(RightMouseAction, ETriggerEvent::Completed, this, &ABasicPlayer::OnRightMouseReleased);
        }
        if (LeftMouseAction)
        {
            EnhancedInputComponent->BindAction(LeftMouseAction, ETriggerEvent::Started,   this, &ABasicPlayer::OnLeftMousePressed);
            EnhancedInputComponent->BindAction(LeftMouseAction, ETriggerEvent::Completed, this, &ABasicPlayer::OnLeftMouseReleased);
        }

        // Mouse wheel zoom
        if (ScrollAction)
        {
            EnhancedInputComponent->BindAction(ScrollAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnScroll);
        }

        // bind start movement simulation
        EnhancedInputComponent->BindAction(StartMovementSimulationAction, ETriggerEvent::Triggered, this, &ABasicPlayer::StartMovementSimulation);
        // bind stop movement simulation
        EnhancedInputComponent->BindAction(StopMovementSimulationAction, ETriggerEvent::Triggered, this, &ABasicPlayer::StopMovementSimulation);
    
    
        if (AttackAction)
        {
            // Bind attack action (you'll need to add AttackAction to your header file)
            EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnAttackInput);
        }

        // Tab-target: cycle to nearest MOB in forward cone
        if (TabTargetAction)
        {
            EnhancedInputComponent->BindAction(TabTargetAction, ETriggerEvent::Started, this, &ABasicPlayer::OnTabTargetInput);
        }

        // Pickup item action
        if (PickupAction)
        {
            EnhancedInputComponent->BindAction(PickupAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnPickupInput);
        }

        // Inventory action
        if (InventoryAction)
        {
            EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnInventoryToggle);
        }

        // Harvest action
        if (HarvestAction)
        {
            EnhancedInputComponent->BindAction(HarvestAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnHarvestInput);
        }

        // Skills panel action
        if (SkillsPanelAction)
        {
            EnhancedInputComponent->BindAction(SkillsPanelAction, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkillsPanelToggle);
        }

        // Individual skill actions
        if (Skill1Action)
        {
            EnhancedInputComponent->BindAction(Skill1Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill1Input);
        }

        if (Skill2Action)
        {
            EnhancedInputComponent->BindAction(Skill2Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill2Input);
        }

        if (Skill3Action)
        {
            EnhancedInputComponent->BindAction(Skill3Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill3Input);
        }

        if (Skill4Action)
        {
            EnhancedInputComponent->BindAction(Skill4Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill4Input);
        }

        if (Skill5Action)
        {
            EnhancedInputComponent->BindAction(Skill5Action, ETriggerEvent::Triggered, this, &ABasicPlayer::OnSkill5Input);
        }

        // Interact with NPC
        if (InteractAction)
        {
            EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ABasicPlayer::OnInteractInput);
            UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: InteractAction bound successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("BasicPlayer: InteractAction is NULL - assign it in the player Blueprint (Details > Input > InteractAction)"));
        }

        // Quest Journal toggle
        if (QuestJournalAction)
        {
            EnhancedInputComponent->BindAction(QuestJournalAction, ETriggerEvent::Started, this, &ABasicPlayer::OnQuestJournalToggle);
        }

        // Equipment toggle
        if (EquipmentAction)
        {
            EnhancedInputComponent->BindAction(EquipmentAction, ETriggerEvent::Started, this, &ABasicPlayer::OnEquipmentToggle);
        }

        // Alt-cursor: toggle mouse cursor on/off
        if (AltCursorAction)
        {
            EnhancedInputComponent->BindAction(AltCursorAction, ETriggerEvent::Started, this, &ABasicPlayer::OnAltCursorToggle);
        }

        // Character stats window toggle
        if (StatsAction)
        {
            EnhancedInputComponent->BindAction(StatsAction, ETriggerEvent::Started, this, &ABasicPlayer::OnStatsToggle);
        }

        // Bestiary window toggle
        if (BestiaryAction)
        {
            EnhancedInputComponent->BindAction(BestiaryAction, ETriggerEvent::Started, this, &ABasicPlayer::OnBestiaryToggle);
        }

        // Titles window toggle
        if (TitlesAction)
        {
            EnhancedInputComponent->BindAction(TitlesAction, ETriggerEvent::Started, this, &ABasicPlayer::OnTitlesToggle);
        }

        // Reputation window toggle
        if (ReputationAction)
        {
            EnhancedInputComponent->BindAction(ReputationAction, ETriggerEvent::Started, this, &ABasicPlayer::OnReputationToggle);
        }

        // Emote list toggle
        if (EmoteListAction)
        {
            EnhancedInputComponent->BindAction(EmoteListAction, ETriggerEvent::Started, this, &ABasicPlayer::OnEmoteListToggle);
        }

        // Main game menu toggle (Escape)
        if (GameMenuAction)
        {
            EnhancedInputComponent->BindAction(GameMenuAction, ETriggerEvent::Started, this, &ABasicPlayer::OnGameMenuToggle);
        }
    }
}

// Sets default values
ABasicPlayer::ABasicPlayer()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    bUseControllerRotationYaw = false;

	//Create audio component
    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);

    // Create spring arm пїЅ controls camera distance and pitch
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength       = DesiredZoom;  // initial distance
    CameraBoom->bUsePawnControlRotation = true;        // arm rotates with controller
    CameraBoom->bEnableCameraLag       = true;
    CameraBoom->CameraLagSpeed         = 8.0f;
    CameraBoom->bEnableCameraRotationLag = false;
    // Only collide with world geometry (walls, terrain) вЂ” ignore pawns (mobs, NPCs, players)
    // so the spring arm does not compress when another character stands between the camera and the player.
    // Use ECC_Camera (not ECC_Visibility) because ECC_Visibility is also used for line-of-sight
    // checks вЂ” mobs/NPCs must still block Visibility for targeting, but must NOT block Camera.
    CameraBoom->ProbeChannel           = ECC_Camera;

    // Prevent other players' capsules from compressing the spring arm.
    // The owning pawn is already auto-ignored by the spring arm probe.
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    }

    // Create the follow camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false; // camera does not rotate пїЅ arm does

	// Create UI Manager component
	UIManager = CreateDefaultSubobject<UUIManager>(TEXT("UIManager"));

	// Create Inventory Manager component  
	InventoryManager = CreateDefaultSubobject<UInventoryManager>(TEXT("InventoryManager"));

	// Create nameplate component пїЅ auto-hidden for local player, visible for remote players
	NameplateComponent = CreateDefaultSubobject<UPlayerNameplateComponent>(TEXT("NameplateComponent"));

	// Create equipment visual component пїЅ attaches item meshes to skeleton sockets
	EquipmentVisualComponent = CreateDefaultSubobject<UEquipmentVisualComponent>(TEXT("EquipmentVisualComponent"));

	// Create emote component - handles emote montage playback, VFX, audio, and interruption
	EmoteComponent = CreateDefaultSubobject<UEmoteComponent>(TEXT("EmoteComponent"));

	// Create cursor interaction component - hover trace, click/double-click, cursor icons, decal management
	CursorInteractionComponent = CreateDefaultSubobject<UCursorInteractionComponent>(TEXT("CursorInteractionComponent"));

	// Create floor-circle decal for when THIS player is targeted by other players
	TargetDecal = CreateDefaultSubobject<UTargetDecalComponent>(TEXT("TargetDecal"));
	TargetDecal->SetupAttachment(RootComponent);

    // Init simulation variables
    SquareCenter = FVector(0.f, 0.f, 90.f); // Assuming Z is up and you want to move around this center
    SideLength = 1000.f; // The length of the side of the square
    TargetPosition = SquareCenter + FVector(SideLength / 2, 0.f, 0.f); // Start with a target position

    // Sync CharacterMovementComponent speed with MoveSpeed so AddMovementInput(dir, 1.0)
    // produces a frame-rate-independent velocity matching the server attribute move_speed.
    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
    {
        CMC->MaxWalkSpeed = MoveSpeed;
        CMC->bOrientRotationToMovement = false;
        CMC->bUseControllerDesiredRotation = false;
    }
}

// Called when the game starts or when spawned
void ABasicPlayer::BeginPlay()
{
Super::BeginPlay();

   // Initialize interpolation targets to the actual spawn location so that
   // UpdateRemotePlayerMovement never sees uninitialized (0,0,0) values before
   // the first SetCoordinates call arrives from the network.
   {
       const FVector SpawnLoc = GetActorLocation();
       LastReceivedPosition   = SpawnLoc;
       TargetReceivedPosition = SpawnLoc;
       LastReceivedRotation   = GetActorRotation();
       TargetReceivedRotation = GetActorRotation();
   }

   MyGameInstance = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(this));
   if (MyGameInstance)
   {
       UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] BasicPlayer::BeginPlay вЂ” CharID=%d isOtherClient=%d Pos=(%.0f,%.0f,%.0f)"),
           playerData.characterData.characterId, (int)playerData.isOtherClient,
           GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
        UE_LOG(LogTemp, Warning, TEXT("GameInstance found"));

        // Route the player's AudioComponent through the SFX SoundClass
        if (AudioComponent && MyGameInstance->AudioManager && MyGameInstance->AudioManager->SFXClass)
        {
            AudioComponent->SoundClassOverride = MyGameInstance->AudioManager->SFXClass;
        }

        // Register in O(1) actor registry so FindTargetActor skips GetAllActorsOfClass
        const int32 ActorId = GetActorId_Implementation();
        if (MyGameInstance->MOBManager && ActorId > 0)
        {
            MyGameInstance->MOBManager->RegisterPlayer(ActorId, this);
        }

        // Remote players: disable physics push вЂ” positions are server-authoritative.
        // Prevents remote capsules from physically blocking the local player's movement.
        // Projectile hits still work because ECC_WorldDynamic is set to Overlap.
        if (playerData.isOtherClient)
        {
            if (UCharacterMovementComponent* CMC = GetCharacterMovement())
            {
                CMC->bEnablePhysicsInteraction = false;
            }
            if (UCapsuleComponent* Capsule = GetCapsuleComponent())
            {
                Capsule->SetCollisionResponseToChannel(ECC_Pawn,         ECR_Ignore);
                Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
            }
        }

        // Register with combat system if this is the local player
        if (!playerData.isOtherClient)
        {
            UCombatSystemManager* CombatManager = MyGameInstance->GetCombatSystemManager();
            if (CombatManager && GetActorId_Implementation() > 0)
            {
                TScriptInterface<ICombatable> CombatableInterface;
                CombatableInterface.SetObject(this);
                CombatableInterface.SetInterface(this);
                
                CombatManager->RegisterCombatable(CombatableInterface);
                UE_LOG(LogTemp, Warning, TEXT("Player %d registered with combat system"), GetActorId_Implementation());
            }
        }
    }
    else
    {
		UE_LOG(LogTemp, Error, TEXT("GameInstance not found"));
	}

	UE_LOG(LogTemp, Warning, TEXT("Player Was Created"));

	// Initialize inventory manager for local player only
	if (!playerData.isOtherClient && MyGameInstance)
	{
		// Set up inventory manager
		if (InventoryManager)
		{
			InventoryManager->SetWorldContext(GetWorld());
			InventoryManager->SetGameInstance(MyGameInstance);

			// Stamp owner character ID BEFORE subscribing so packet filter is active from the first message.
			if (playerData.characterData.characterId > 0)
			{
				InventoryManager->SetOwnerCharacterId(playerData.characterData.characterId);
			}
			
			// Get network manager from game instance and initialize inventory
			if (UNetworkManager* NetworkManager = MyGameInstance->GetNetworkManager())
			{
				InventoryManager->Initialize(NetworkManager);
				InventoryManager->SubscribeToNetworkManager();
			}
			
			// Set reference in game instance for easy access
			MyGameInstance->SetInventoryManager(InventoryManager);

			// Link EquipmentManager so it can sync is_equipped flags on EQUIPMENT_STATE / EQUIP_RESULT
			if (UEquipmentManager* EquipMgr = MyGameInstance->GetEquipmentManager())
			{
				EquipMgr->SetInventoryManager(InventoryManager);
			}
		}

		// Initialize harvest manager
		if (UHarvestManager* HarvestManager = MyGameInstance->GetHarvestManager())
		{
			HarvestManager->SetWorldContext(GetWorld());
			HarvestManager->SetGameInstance(MyGameInstance);

			// Get network manager from game instance and initialize harvest
			if (UNetworkManager* NetworkManager = MyGameInstance->GetNetworkManager())
			{
				HarvestManager->Initialize(NetworkManager);
				HarvestManager->SubscribeToNetworkManager();
			}

			UE_LOG(LogTemp, Warning, TEXT("HarvestManager initialized for local player"));
		}

		// Initialize UI manager with slight delay to ensure everything is ready
		if (UIManager)
		{
			GetWorld()->GetTimerManager().SetTimer(UIInitTimerHandle, [this]()
			{
				// Guard: skip UI initialization for other-client (remote) players.
				// isOtherClient is false at BeginPlay time but may be set to true before
				// the timer fires when this actor represents another connected client.
				if (playerData.isOtherClient)
				{
					return;
				}

				if (InventoryManager)
				{
					// Get PlayerController
					APlayerController* PC = Cast<APlayerController>(GetController());
					
					// Get HarvestManager from GameInstance
					UHarvestManager* HarvestManager = MyGameInstance ? MyGameInstance->GetHarvestManager() : nullptr;

				//get ExperienceManager from GameInstance
					UExperienceManager* ExperienceManager = MyGameInstance ? MyGameInstance->GetExperienceManager() : nullptr;

					// Subscribe to level-up events so we can play LevelUpSound
					if (ExperienceManager && !playerData.isOtherClient)
					{
						ExperienceManager->OnLevelUp.AddDynamic(this, &ABasicPlayer::HandleLevelUp);
					}

					// Subscribe to effectTick to show floating heal/regen numbers above this player.
					// Applied for ALL players (both local and others) so HoT ticks on remote
					// characters also show a number floating over their heads.
					if (MyGameInstance && MyGameInstance->PlayerStatsNetworkHandler)
					{
						MyGameInstance->PlayerStatsNetworkHandler->OnEffectTick.AddDynamic(
							this, &ABasicPlayer::HandleEffectTickFCT);
					}
					
					// Get SkillManager from GameInstance
					UPlayerSkillManager* SkillManager = MyGameInstance ? MyGameInstance->GetPlayerSkillManager() : nullptr;
					
					// Initialize UIManager with all managers
					UIManager->Initialize(InventoryManager, HarvestManager, ExperienceManager, SkillManager);
					
					// Set PlayerController reference immediately after Initialize
					if (PC)
					{
						UIManager->SetPlayerController(PC);
						UE_LOG(LogTemp, Warning, TEXT("UIManager: PlayerController set during initialization"));

						UIManager->InitFTCManager(PC);
						UE_LOG(LogTemp, Warning, TEXT("UIManager: Init called for FCT system"));

                        PlayerHUD = UIManager->GetPlayerInterfaceWidget()->GetPlayerHUD();

						// Flush any stats that arrived before the HUD was ready.
						RefreshHUD();

						// Subscribe to StatsManager so effectTick and other paths that
						// bypass ProcessStatsUpdate still update playerData and the HUD.
						if (UPlayerStatsManager* StatsMgr = MyGameInstance ? MyGameInstance->GetPlayerStatsManager() : nullptr)
						{
							StatsMgr->OnStatsUpdated.AddDynamic(this, &ABasicPlayer::HandleStatsManagerUpdate);

							// stats_update packets arrive before UIManager is ready (boundListeners=0 at
							// broadcast time). Replay the cached stats now so AEW shows effects immediately.
							const FPlayerStatsUpdateStruct& Cached = StatsMgr->GetCachedStats();
							if (Cached.characterId > 0)
							{
								HandleStatsManagerUpdate(Cached);
							}
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("UIManager: Failed to get PlayerController during initialization"));
					}
					
					UE_LOG(LogTemp, Warning, TEXT("UIManager initialized with all managers including SkillManager"));

					// Initialize experience widget for this character.
					// ExperienceManager already has correct progression data
					// (including expForCurrentLevel) set by SpawnPlayerForClient
					// before BeginPlay fires.
					if (ExperienceManager && playerData.characterData.characterId > 0)
					{
						UIManager->InitializeExperienceWidget(playerData.characterData.characterId);

						UE_LOG(LogTemp, Warning, TEXT("ExperienceWidget initialized for character %d"), 
							playerData.characterData.characterId);
					}

					// Initialize skill widgets if SkillManager is available
					if (SkillManager)
					{
						UIManager->InitializeSkillWidgets();
						UE_LOG(LogTemp, Warning, TEXT("Skill widgets initialized"));
					}

				// Bind dialogue and quest widgets to their managers
					UDialogueManager* DlgMgr   = MyGameInstance ? MyGameInstance->GetDialogueManager()  : nullptr;
					UQuestManager*    QuestMgr  = MyGameInstance ? MyGameInstance->GetQuestManager()     : nullptr;
					UIManager->InitializeDialogueAndQuestWidgets(DlgMgr, QuestMgr);

				// Bind equipment / vendor / repair / trade widgets to their managers
					UIManager->InitializeItemSystemWidgets(
						MyGameInstance ? MyGameInstance->GetEquipmentManager() : nullptr,
						MyGameInstance ? MyGameInstance->GetVendorManager()    : nullptr,
						MyGameInstance ? MyGameInstance->GetRepairManager()    : nullptr,
						MyGameInstance ? MyGameInstance->GetTradeManager()     : nullptr);

			// Bind skill shop widget to the skill shop manager
				UIManager->InitializeSkillShopWidget(
					MyGameInstance ? MyGameInstance->GetSkillShopManager() : nullptr);

			// Bind character stats widget to the stats manager
				UIManager->InitializeStatsWidget(
					MyGameInstance ? MyGameInstance->GetPlayerStatsManager() : nullptr);

				// Bind progression managers (title + mastery) and character name to stats widget
				if (UPlayerStatsWidget* SW = UIManager->GetPlayerStatsWidget())
				{
					SW->BindToProgressionManagers(
						MyGameInstance ? MyGameInstance->GetTitleManager()   : nullptr,
						MyGameInstance ? MyGameInstance->GetMasteryManager() : nullptr);
					SW->SetCharacterName(playerData.characterData.characterName);
				}

				// Bind titles window
				UIManager->InitializeTitlesWidget(
					MyGameInstance ? MyGameInstance->GetTitleManager()          : nullptr,
					MyGameInstance ? MyGameInstance->GetTitleNetworkHandler()   : nullptr,
					playerData.characterData.characterId);

				// Bind reputation window
				UIManager->InitializeReputationWidget(
					MyGameInstance ? MyGameInstance->GetReputationManager() : nullptr);

				// Bind emote list window
				UIManager->InitializeEmoteListWidget(
					MyGameInstance ? MyGameInstance->GetEmoteManager()        : nullptr,
					MyGameInstance ? MyGameInstance->GetEmoteNetworkHandler() : nullptr,
					playerData.characterData.characterId);

				// Initialize world notification system (bestiary + toast/zone/etc.)
				if (MyGameInstance && MyGameInstance->GetBestiaryNetworkHandler())
					{
						UIManager->InitializeNotificationSystem(MyGameInstance->GetBestiaryNetworkHandler());
						UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: WorldNotificationManager initialized"));

						// Request the bestiary overview so the mob list is populated on login
						if (playerData.characterData.characterId > 0)
						{
							MyGameInstance->GetBestiaryNetworkHandler()->RequestBestiaryOverview(
								playerData.characterData.characterId);
							UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Requested bestiary overview for character %d"),
								playerData.characterData.characterId);
						}
					}

				// Initialize WIO (World Interactive Objects) widgets
				if (MyGameInstance && MyGameInstance->GetWorldObjectManager())
				{
					UIManager->InitializeWIOWidgets(MyGameInstance->GetWorldObjectManager());
					UE_LOG(LogTemp, Log, TEXT("BasicPlayer: WIO widgets initialized"));
				}

			// Subscribe to weight status and initialize equipment visuals
				if (UEquipmentManager* EqMgr = MyGameInstance ? MyGameInstance->GetEquipmentManager() : nullptr)
				{
					EqMgr->OnWeightStatusChangedDelegate.AddDynamic(this, &ABasicPlayer::HandleWeightStatusChanged);
					UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Subscribed to weight status events"));

					// Initialize equipment visual component so equipped items appear on the mesh
					if (EquipmentVisualComponent)
					{
						UItemManager* ItemMgr = MyGameInstance->GetItemManager();
						EquipmentVisualComponent->Initialize(EqMgr, ItemMgr);
						UE_LOG(LogTemp, Log, TEXT("BasicPlayer: EquipmentVisualComponent initialized"));
					}

					// Explicitly request equipment state from server.
					// EQUIPMENT_STATE may arrive before the visual component binds its delegate,
					// but Initialize() above already replays the cached state.
					// This request ensures we get up-to-date data even if the server
					// does not push EQUIPMENT_STATE automatically on join.
					if (playerData.characterData.characterId > 0)
					{
						EqMgr->RequestGetEquipment(playerData.characterData.characterId);
						UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Requested equipment state for character %d"),
							playerData.characterData.characterId);
					}
				}

				// Subscribe to ChatManager: routes each received message to the sender's nameplate bubble.
				if (UChatManager* ChatMgr = MyGameInstance ? MyGameInstance->GetChatManager() : nullptr)
				{
					ChatMgr->OnChatMessageReceived.AddDynamic(this, &ABasicPlayer::HandleChatMessageForBubble);
				}

				// For the LOCAL player: when they equip/remove a title, push the new display name
				// to their nameplate so other viewers see it immediately.
				if (!playerData.isOtherClient)
				{
					if (UTitleManager* TitleMgr = MyGameInstance ? MyGameInstance->GetTitleManager() : nullptr)
					{
						TitleMgr->OnTitlesUpdated.AddDynamic(this, &ABasicPlayer::HandleTitlesUpdated);
					}
				}

				// The death screen will be shown automatically via the pending mechanism
				// in UIManager if the character spawned as dead (SetDead_Implementation
				// already called in SpawnPlayerForClient before this timer fires).
				}

				// Subscribe to UIManager's deferred ready signal.
				// OnUIManagerInitialized is broadcast one game-thread tick AFTER
				// PlayerInterfaceWidget->AddToViewport(), which guarantees the
				// render thread has already received the widget before we signal
				// the loading-screen countdown to start.
				UIManager->OnUIManagerInitialized.AddDynamic(this, &ABasicPlayer::HandleUIManagerInitialized);
				UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] UIInitTimer: subscribed to OnUIManagerInitialized"));
                
			}, 0.5f, false);
		}
	}
}

//Create HUD
void ABasicPlayer::CreateHUD()
{
    //RefreshHUD();
}

void ABasicPlayer::HandleLevelUp(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience)
{
    if (playerData.isOtherClient) return;

    UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Level up! %d -> %d"), OldLevel, NewLevel);
    if (const FEntityAudioProfile* Profile = GetAudioProfile())
    {
        PlayEventSound(Profile->LevelUp);
    }
}

void ABasicPlayer::HandleEffectTickFCT(const FEffectTickData& TickData)
{
    // Only process ticks that belong to this specific player actor.
    if (TickData.characterId != playerData.characterData.characterId) return;

    // Heal-over-time → show green "+N" via the normal healing path
    // (ShowHealingEffect_Implementation handles FCT + optional skill sounds/VFX).
    if (TickData.effectTypeSlug == TEXT("hot") && TickData.value > 0)
    {
        ShowHealingEffect_Implementation(TickData.value, TickData.effectSlug);
    }
}

void ABasicPlayer::HandleChatMessageForBubble(const FChatMessageStruct& Message)
{
    // Ignore error / system messages.
    if (Message.bIsError) return;
    if (Message.senderId <= 0) return;

    // Find the player actor who sent this message and show their speech bubble.
    // Only remote players are registered in NameplateManager, so the local player's
    // own messages are silently skipped (no nameplate entry for bIsLocalPlayer actors).
    if (!MyGameInstance) return;

    ABasicPlayer* Sender = MyGameInstance->GetPlayerByCharacterId(Message.senderId);
    if (!Sender || !Sender->GetNameplateComponent()) return;

    Sender->GetNameplateComponent()->ShowChatBubble(Message.text, ChatBubbleDisplayDuration);
}

void ABasicPlayer::HandleTitlesUpdated(const FPlayerTitlesState& State)
{
    // Push the currently-equipped title name to this actor's nameplate.
    // The display name is empty when no title is equipped → nameplate hides the title row.
    if (NameplateComponent)
    {
        NameplateComponent->UpdateTitle(State.equippedTitle.displayName);
    }
}

void ABasicPlayer::SetEquippedTitle(const FString& TitleDisplayName)
{
    // Called by SpawnPlayerForClient (or a future title-broadcast handler) to set the
    // initial / updated title for a remote player's nameplate.
    if (NameplateComponent)
    {
        NameplateComponent->UpdateTitle(TitleDisplayName);
    }
}

void ABasicPlayer::HandleUIManagerInitialized()
{
    // Unsubscribe immediately вЂ” this must fire exactly once per spawn.
    if (UIManager)
    {
        UIManager->OnUIManagerInitialized.RemoveDynamic(this, &ABasicPlayer::HandleUIManagerInitialized);
    }

    // At this point PlayerInterfaceWidget->AddToViewport() was called one game-thread
    // tick ago, so the render thread has already received the draw command for the
    // game UI. It is now safe to signal the loading-screen countdown gates.
    bUIInitDone = true;

    if (MyGameInstance)
    {
        MyGameInstance->NotifyUIInitialized();
    }

    // Bind cursor interaction delegates now that UI is ready.
    if (CursorInteractionComponent)
    {
        CursorInteractionComponent->OnSingleClicked.AddDynamic(
            this, &ABasicPlayer::DispatchCursorSelect);
        CursorInteractionComponent->OnDoubleClicked.AddDynamic(
            this, &ABasicPlayer::DispatchCursorInteract);
    }

    // Subscribe to WIO actor spawns so we can track proximity changes
    if (MyGameInstance)
    {
        if (UWorldObjectManager* WOM = MyGameInstance->GetWorldObjectManager())
        {
            WOM->OnWorldObjectActorSpawned.AddDynamic(this, &ABasicPlayer::HandleWIOActorSpawned);

            // Also bind existing actors (in case they spawned before UI was ready)
            for (AWorldInteractiveObjectActor* Actor : WOM->GetAllObjectActors())
            {
                if (Actor)
                {
                    Actor->OnProximityChanged.AddDynamic(this, &ABasicPlayer::HandleWIOProximityChanged);
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] HandleUIManagerInitialized: bUIInitDone=true, NotifyUIInitialized sent. Pos=(%.0f,%.0f,%.0f)"),
        GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
}

// в”Ђв”Ђв”Ђ IWorldInteractable (remote player targeting) в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ

EInteractableType ABasicPlayer::GetInteractableType() const
{
    // Only other clients appear as interactable remote players.
    return playerData.isOtherClient ? EInteractableType::RemotePlayer : EInteractableType::None;
}

FText ABasicPlayer::GetInteractableDisplayName() const
{
    const FString Name  = playerData.characterData.characterName;
    const int32   Level = playerData.characterData.characterLevel;
    if (Level > 0)
        return FText::FromString(FString::Printf(TEXT("%s  [%d]"), *Name, Level));
    return FText::FromString(Name);
}

// в”Ђв”Ђв”Ђ Cursor Interaction в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ

bool ABasicPlayer::IsUIBlockingInteraction() const
{
    // Use HasUIWindowOpen() вЂ” not ShouldShowCursor() вЂ” so that cursor world interaction
    // is never blocked by bAltCursorActive (which is true whenever the cursor is shown).
    return UIManager && UIManager->HasUIWindowOpen();
}

float ABasicPlayer::GetInteractionRange() const
{
    return CursorInteractionComponent ? CursorInteractionComponent->GetInteractionRange() : 280.f;
}

void ABasicPlayer::DispatchCursorSelect(AActor* Target, EInteractableType Type)
{
    if (!Target)
    {
        // Click on empty ground в†’ clear all locks
        ClearLockedTarget();
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(nullptr, EInteractableType::None);
        return;
    }

    switch (Type)
    {
    case EInteractableType::MOB_Alive:
    case EInteractableType::MOB_Harvestable:
    case EInteractableType::MOB_Harvested:
        if (ABasicMOB* Mob = Cast<ABasicMOB>(Target))
        {
            // Select only вЂ” no auto-attack on single click
            SetLockedTarget(Mob);
        }
        break;

    case EInteractableType::NPC:
    case EInteractableType::DroppedItem:
    case EInteractableType::RemotePlayer:
        // Visual lock so the floor circle shows, no combat action
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(Target, Type);
        break;

    default:
        break;
    }
}

void ABasicPlayer::DispatchCursorInteract(AActor* Target, EInteractableType Type)
{
    if (!Target) return;

    const float Range = GetInteractionRange();

    // Measure from the nearest point on the target's capsule (or actor origin as fallback)
    // so the player stops at the surface of the actor, not at its pivot (which can be
    // at chest/mid height, causing the player to stop 60-80 cm too far away).
    float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
    if (const ACharacter* TargetChar = Cast<ACharacter>(Target))
    {
        if (const UCapsuleComponent* Cap = TargetChar->GetCapsuleComponent())
        {
            Dist = FMath::Max(0.f, Dist - Cap->GetScaledCapsuleRadius());
        }
    }
    const bool bInRange = (Dist <= Range);

    switch (Type)
    {
    // в”Ђв”Ђ MOB: alive в†’ attack в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::MOB_Alive:
        if (ABasicMOB* Mob = Cast<ABasicMOB>(Target))
        {
            SetLockedTarget(Mob);
            if (bInRange)
            {
                bIsAutoAttacking = true;
                DoAutoAttack();
            }
            else
            {
                PendingInteraction       = EPendingInteraction::AutoAttack;
                PendingInteractionTarget = Target;
                bIsApproachingTarget     = true;
            }
        }
        break;

    // в”Ђв”Ђ MOB: harvestable / harvested в†’ harvest or inspect в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::MOB_Harvestable:
    case EInteractableType::MOB_Harvested:
        if (ABasicMOB* Mob = Cast<ABasicMOB>(Target))
        {
            SetLockedTarget(Mob);
            if (bInRange)
            {
                UHarvestManager* HMgr = MyGameInstance ? MyGameInstance->GetHarvestManager() : nullptr;
                if (HMgr)
                    HMgr->TryHarvestSpecificCorpse(Mob);
            }
            else
            {
                PendingInteraction       = EPendingInteraction::Harvest;
                PendingInteractionTarget = Target;
                bIsApproachingTarget     = true;
            }
        }
        break;

    // в”Ђв”Ђ NPC в†’ open dialogue в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::NPC:
        if (ABasicNPC* NPC = Cast<ABasicNPC>(Target))
        {
            if (CursorInteractionComponent)
                CursorInteractionComponent->SetVisualLock(Target, Type);

            UDialogueManager* DlgMgr = MyGameInstance ? MyGameInstance->GetDialogueManager() : nullptr;
            if (bInRange)
            {
                if (DlgMgr)
                {
                    DlgMgr->OpenDialogue(NPC->GetNPCId());
                }
            }
            else
            {
                PendingInteraction       = EPendingInteraction::TalkNPC;
                PendingInteractionTarget = Target;
                bIsApproachingTarget     = true;
            }
        }
        break;

    // в”Ђв”Ђ DroppedItem в†’ pick up в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::DroppedItem:
        if (ADroppedItemActor* Item = Cast<ADroppedItemActor>(Target))
        {
            if (CursorInteractionComponent)
                CursorInteractionComponent->SetVisualLock(Target, Type);

            if (bInRange)
            {
                if (InventoryManager)
                    InventoryManager->PickupSpecificItem(Item);
            }
            else
            {
                PendingInteraction       = EPendingInteraction::PickupItem;
                PendingInteractionTarget = Target;
                bIsApproachingTarget     = true;
            }
        }
        break;

    // в”Ђв”Ђ RemotePlayer в†’ inspect (placeholder) в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
    case EInteractableType::RemotePlayer:
        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(Target, Type);
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Double-click on remote player вЂ” inspect not yet implemented"));
        break;

    default:
        break;
    }
}

void ABasicPlayer::DispatchPendingInteraction()
{
    AActor* Target = PendingInteractionTarget.Get();

    // Reset state first so re-entrant calls don't loop
    const EPendingInteraction Pending = PendingInteraction;
    PendingInteraction       = EPendingInteraction::None;
    PendingInteractionTarget = nullptr;

    if (!Target) return;

    switch (Pending)
    {
    case EPendingInteraction::AutoAttack:
        bIsAutoAttacking = true;
        DoAutoAttack();
        break;

    case EPendingInteraction::Harvest:
        {
            UHarvestManager* HMgr = MyGameInstance ? MyGameInstance->GetHarvestManager() : nullptr;
            if (HMgr)
                HMgr->TryHarvestSpecificCorpse(Cast<ABasicMOB>(Target));
        }
        break;

    case EPendingInteraction::TalkNPC:
        {
            UDialogueManager* DlgMgr = MyGameInstance ? MyGameInstance->GetDialogueManager() : nullptr;
            if (DlgMgr)
            {
                if (ABasicNPC* NPC = Cast<ABasicNPC>(Target))
                    DlgMgr->OpenDialogue(NPC->GetNPCId());
            }
        }
        break;

    case EPendingInteraction::PickupItem:
        if (InventoryManager)
            InventoryManager->PickupSpecificItem(Cast<ADroppedItemActor>(Target));
        break;

    default:
        break;
    }
}

void ABasicPlayer::HandleWeightStatusChanged(const FWeightStatusData& WeightStatus){
    // Weight status is informational only (used by UI weight bar).
    // Movement speed is fully server-authoritative and arrives via move_speed in stats_update.
    // No client-side speed penalty is applied here.
    UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Weight status updated вЂ” current=%.1f limit=%.1f overweight=%d (speed unchanged)"),
        WeightStatus.currentWeight, WeightStatus.weightLimit, (int)WeightStatus.isOverweight);
}

// update HUD
void ABasicPlayer::UpdateHUD()
{
    if (PlayerHUD)
    {
        if (!GetIsOtherClient())
        {
            // Only update HP/MP bars when we have real server-authoritative max values
            // (populated by the first stats_update). Before that keep the bars hidden/empty
            // so we don't show a fake full HP bar on join.
            const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health"));
            const FAttributeDataStruct* ManaAttr   = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana"));

            if (HealthAttr && HealthAttr->attributeValue > 0 &&
                ManaAttr   && ManaAttr->attributeValue   >= 0)
            {
                PlayerHUD->SetHP(playerData.characterData.characterCurrentHealth,
                                 static_cast<float>(HealthAttr->attributeValue));
                PlayerHUD->SetMana(playerData.characterData.characterCurrentMana,
                                   static_cast<float>(ManaAttr->attributeValue));
            }
            // else: bars remain at their last known state until stats_update arrives
        }
    }
}

// Called every frame
void ABasicPlayer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // First-Tick spawn gate: fires NotifyPlayerSpawned exactly once, only after
    // UIInitTimer has completed (bUIInitDone) and the camera has had at least
    // one real game frame to settle into the correct position behind the character.
    if (!playerData.isOtherClient && bUIInitDone && !bSpawnNotified && MyGameInstance)
    {
        bSpawnNotified = true;
        UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] Tick: NotifyPlayerSpawned fired. Pawn Pos=(%.0f,%.0f,%.0f)"),
            GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
        MyGameInstance->NotifyPlayerSpawned();
    }

    // Log camera world position for the first 10 ticks of the local player to
    // diagnose any residual "camera in ground" artifact after loading screen clears.
    if (!playerData.isOtherClient && FollowCamera)
    {
        static int32 sCamLogCount = 0;
        if (sCamLogCount < 10)
        {
            ++sCamLogCount;
            APlayerController* DbgPC = Cast<APlayerController>(GetController());
            FRotator CR = DbgPC ? DbgPC->GetControlRotation() : FRotator::ZeroRotator;
            FVector  CamLoc = FollowCamera->GetComponentLocation();
            UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] CamTick#%d CameraLoc=(%.0f,%.0f,%.0f) ControlRot=(P=%.1f Y=%.1f) bUIInitDone=%d bSpawnNotified=%d"),
                sCamLogCount, CamLoc.X, CamLoc.Y, CamLoc.Z, CR.Pitch, CR.Yaw,
                bUIInitDone ? 1 : 0, bSpawnNotified ? 1 : 0);
        }
    }

    FVector CurrentLocation = GetActorLocation();
    const bool bWasMoving = playerData.characterData.bIsMoving;
    playerData.characterData.bIsMoving = !CurrentLocation.Equals(LastFrameLocation, 1.0f);
    LastFrameLocation = CurrentLocation;

    // Interrupt emote when the character starts moving
    if (!bWasMoving && playerData.characterData.bIsMoving && EmoteComponent)
    {
        EmoteComponent->NotifyMovementStarted();
    }

    if (MyGameInstance && !playerData.isOtherClient)
    {
        HandleMouseButtonsMoveForward();
        ClampControlPitch();

        // Update player movement for local player
		UpdateCurrentPlayerMovement(DeltaTime);
        // Smoothly rotate mesh toward DesiredMeshYaw
        UpdateMeshRotation(DeltaTime);
        // Drive approach movement every frame so CharacterMovementComponent gets
        // AddMovementInput on the same frame it is consumed пїЅ no more 1mm jitter.
        UpdateApproach(DeltaTime);
        // Smoothly interpolate camera zoom
        if (CameraBoom)
        {
            CameraBoom->TargetArmLength = FMath::FInterpTo(
                CameraBoom->TargetArmLength, DesiredZoom, DeltaTime, ZoomInterpSpeed);
        }

        // Throttled check: close NPC windows if player has walked too far from the NPC.
        NpcDistCheckAccum += DeltaTime;
        if (NpcDistCheckAccum >= 0.5f)
        {
            NpcDistCheckAccum = 0.0f;
            CheckNPCInteractionDistance();
        }
    }

    // Update player movement for remote player
    if (playerData.isOtherClient)
    {
        // UpdateRemotePlayerMovementOld(DeltaTime);
        UpdateRemotePlayerMovement();
    }

    // Simulate movement for local player
    if (bSimulateMovement && !playerData.isOtherClient)
    {
        UpdateMovementSimulation(DeltaTime);
	}

	//if character data is not empty
	if (playerData.characterData.characterId != 0) {
		// Update the HUD
		UpdateHUD();
	}

    CheckForMOB();
    CheckForNPC();

    // Keep the locked-target frame in sync every Tick
    if (LockedTarget && UIManager && !playerData.isOtherClient)
    {
        const int32 CurrentHP = LockedTarget->GetMOBCurrentHealth();
        const int32 MaxHP = LockedTarget->GetMOBAttributes().attributesData.Contains(TEXT("max_health"))
            ? LockedTarget->GetMOBAttributes().attributesData[TEXT("max_health")].attributeValue
            : 100;
        UIManager->UpdateMobTargetFrameHP(CurrentHP, MaxHP);

        // Re-call SetMobInfo only when the aggro state has actually flipped
        const bool bCurrentAggro = LockedTarget->GetMOBIsAggressive();
        if (bCurrentAggro != bLastKnownTargetAggro)
        {
            bLastKnownTargetAggro = bCurrentAggro;

            UIManager->ShowMobTargetFrame(
                LockedTarget->GetMOBData().mobSlug,
                LockedTarget->GetMobName(),
                LockedTarget->GetMOBLevel(),
                CurrentHP, MaxHP,
                bCurrentAggro,
                LockedTarget->CachedIcon);
        }
    }
}

void ABasicPlayer::Move(const FInputActionValue& Value)
{
    if (Controller == nullptr) return;
    if (bIsCasting) return;

    // Cancel WIO channel on any movement input
    CancelWIOChannelIfActive();

    // Manual WASD input interrupts the auto-attack cycle and any approach walk,
    // but keeps the target lock so the player can reposition during combat.
    if (bIsAutoAttacking || bIsApproachingTarget)
    {
        UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Auto-attack interrupted by manual movement"));
        StopAutoAttack();
    }

    const FVector2D MoveValue = Value.Get<FVector2D>(); // NOT normalized пїЅ keep X/Y separate
    const float CameraYaw = Controller->GetControlRotation().Yaw;

    if (bIsRightMouseDown)
    {
        // RMB mode: movement is relative to camera (W/S forward/back, A/D strafe)
        const FRotator ControlYaw(0, CameraYaw, 0);
        const FVector Forward = ControlYaw.RotateVector(FVector::ForwardVector);
        const FVector Right   = ControlYaw.RotateVector(FVector::RightVector);

        const FVector Input = (Forward * MoveValue.Y + Right * MoveValue.X).GetSafeNormal();
        AddMovementInput(Input, 1.0f);

        // During a skill animation the mesh is locked toward the target — do not
        // let RMB drag rotate the character away from the enemy mid-cast.
        const bool bAnimLocked = MyGameInstance && MyGameInstance->GetPlayerSkillManager()
            && MyGameInstance->GetPlayerSkillManager()->IsSkillAnimationPlaying();
        if (!bAnimLocked)
        {
            DesiredMeshYaw = CameraYaw;
            bHasDesiredMeshYaw = true;
        }
    }
    else
    {
        // Keyboard-turn mode (no RMB): W/S move by mesh facing, A/D rotate mesh
        if (!FMath::IsNearlyZero(MoveValue.Y))
        {
            const FVector MeshForward = GetActorForwardVector();
            AddMovementInput(MeshForward, MoveValue.Y > 0.f ? 1.0f : -1.0f);
        }

        if (!FMath::IsNearlyZero(MoveValue.X))
        {
            const float TurnAmount = MoveValue.X * MeshRotationSpeed * GetWorld()->GetDeltaSeconds();
            DesiredMeshYaw = GetActorRotation().Yaw + TurnAmount;
            bHasDesiredMeshYaw = true;
        }
    }
}

void ABasicPlayer::Look(const FInputActionValue& Value)
{
    // Only process mouse look when at least one mouse button is held (WoW-style)
    if (!bIsRightMouseDown && !bIsLeftMouseDown) return;

    // Dead players cannot rotate вЂ” the corpse must stay still.
    if (playerData.characterData.bIsDead) return;

    if (Controller != nullptr)
    {
        const FVector2D LookValue = Value.Get<FVector2D>();

        // в”Ђв”Ђ LMB-only drag detection в”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђв”Ђ
        // When only LMB is held (no RMB), accumulate pixel movement.  Once the
        // drag threshold is exceeded we capture the mouse and mark the press as a
        // drag so OnLeftMouseReleased won't fire HandleConfirmedClick.
        if (bIsLeftMouseDown && !bIsRightMouseDown && !bLMBDragActive)
        {
            const float DragThreshold = (CursorInteractionComponent && CursorInteractionComponent->GetEffectiveConfig())
                ? CursorInteractionComponent->GetEffectiveConfig()->DragThresholdPixels
                : 8.f;

            LMBDragPixelsAccum += LookValue.Size();
            if (LMBDragPixelsAccum >= DragThreshold)
            {
                bLMBDragActive = true;
                ApplyMouseCaptureIfNoUIOpen();
                if (CursorInteractionComponent)
                {
                    CursorInteractionComponent->NotifyDragStarted();
                }
            }
        }

        if (!FMath::IsNearlyZero(LookValue.X))
        {
            AddControllerYawInput(LookValue.X * CameraYawSensitivity);

            // RMB held: character follows camera yaw — but not mid-cast.
            // The camera itself still rotates freely; only the mesh stays locked
            // toward the target until the animation finishes.
            if (bIsRightMouseDown)
            {
                const bool bAnimLocked = MyGameInstance && MyGameInstance->GetPlayerSkillManager()
                    && MyGameInstance->GetPlayerSkillManager()->IsSkillAnimationPlaying();
                if (!bAnimLocked)
                {
                    DesiredMeshYaw = Controller->GetControlRotation().Yaw;
                    bHasDesiredMeshYaw = true;
                }
            }
        }

        if (!FMath::IsNearlyZero(LookValue.Y))
        {
            AddControllerPitchInput(LookValue.Y * -CameraPitchSensitivity);
        }
    }
}

void ABasicPlayer::OnRightMousePressed()
{
    bIsRightMouseDown = true;
    ApplyMouseCaptureIfNoUIOpen();

    // Disable hover trace during RMB camera drag вЂ” no point doing it when cursor is invisible.
    if (CursorInteractionComponent)
    {
        CursorInteractionComponent->SetHoverTraceEnabled(false);
    }
}

void ABasicPlayer::OnRightMouseReleased()
{
    bIsRightMouseDown = false;

    if (!bIsLeftMouseDown)
    {
        RestoreCursorToUIManager();
    }

    // Re-enable hover trace now that the cursor is visible again.
    if (CursorInteractionComponent)
    {
        CursorInteractionComponent->SetHoverTraceEnabled(true);
    }
}

void ABasicPlayer::OnLeftMousePressed()
{
    bIsLeftMouseDown = true;

    // Record press state for click-vs-drag detection.
    // We do NOT capture the mouse immediately (unlike RMB) вЂ” instead we wait to
    // see if the player drags (Look() accumulates movement and fires the capture
    // once the threshold is exceeded).  This lets a clean click-release reach
    // HandleConfirmedClick so CursorInteractionComponent can fire select/interact.
    LMBPressTime        = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    LMBDragPixelsAccum  = 0.f;
    bLMBDragActive      = false;
}

void ABasicPlayer::OnLeftMouseReleased()
{
    bIsLeftMouseDown = false;

    if (!bIsRightMouseDown)
    {
        RestoreCursorToUIManager();
    }

    // Click confirmed: short press with no meaningful drag and no UI window blocking input.
    if (!bLMBDragActive && CursorInteractionComponent && !IsUIBlockingInteraction())
    {
        CursorInteractionComponent->HandleConfirmedClick();
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("CursorInteraction: LMB click BLOCKED вЂ” drag=%d, comp=%s, UIBlocking=%d"),
            bLMBDragActive ? 1 : 0,
            CursorInteractionComponent ? TEXT("OK") : TEXT("NULL"),
            IsUIBlockingInteraction() ? 1 : 0);
        if (bLMBDragActive && CursorInteractionComponent)
        {
            // Drag ended вЂ” signal the component to break any pending double-click chain.
            CursorInteractionComponent->NotifyDragStarted(); // resets double-click window
        }
    }

    bLMBDragActive = false;
}

void ABasicPlayer::OnScroll(const FInputActionValue& Value)
{
    if (!CameraBoom) return;

    // Don't zoom if any UI window is open вЂ” let the scroll reach the widget
    if (UIManager && UIManager->HasUIWindowOpen()) return;

    // Value is a float: positive = scroll up (zoom in), negative = scroll down (zoom out)
    const float ScrollDelta = Value.Get<float>();
    DesiredZoom = FMath::Clamp(DesiredZoom - ScrollDelta * ZoomStep, ZoomMin, ZoomMax);
}

void ABasicPlayer::ApplyMouseCaptureIfNoUIOpen()
{
    // Don't capture the mouse if any UI window is open - the player needs
    // the cursor to interact with inventory, vendor, etc.
    if (UIManager && UIManager->HasUIWindowOpen()) return;

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void ABasicPlayer::RestoreCursorToUIManager()
{
    // Hand cursor and input mode back to UIManager so it re-applies
    // its own state (show if any window is open, hide otherwise).
    if (UIManager)
    {
        UIManager->UpdateCursorAndInputMode();
        return;
    }

    // Fallback if UIManager is not available
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI());
    }
}

void ABasicPlayer::UpdateMeshRotation(float DeltaTime)
{
    if (!bHasDesiredMeshYaw) return;

    const FRotator Current = GetActorRotation();
    const FRotator Target(Current.Pitch, DesiredMeshYaw, Current.Roll);

    // MeshRotationSpeed is in degrees/sec.
    // Convert to RInterpTo speed: angular_speed_deg_per_sec / 90 gives a value
    // that is way too low (180/90=2). Instead use a fixed responsive interp speed
    // that covers any remaining angle in ~1-2 frames at normal frame rates.
    // For keyboard A/D turning the delta is tiny each frame so this is instant;
    // for RMB camera snap the delta can be up to 180В° and we want ~100ms to close it.
    const float InterpSpeed = 15.0f;
    const FRotator Smoothed = FMath::RInterpTo(Current, Target, DeltaTime, InterpSpeed);
    SetActorRotation(Smoothed);

    // Stop interpolating once we are close enough
    if (FMath::Abs(FRotator::NormalizeAxis(Smoothed.Yaw - DesiredMeshYaw)) < 0.5f)
    {
        SetActorRotation(Target);
        bHasDesiredMeshYaw = false;
    }
}

void ABasicPlayer::HandleMouseButtonsMoveForward()
{
    if (!bEnableMouseButtonsMoveForward) return;
    if (!bIsLeftMouseDown || !bIsRightMouseDown) return;
    if (playerData.characterData.bIsDead || bIsPickingUp || bIsCasting) return;
    if (UIManager && UIManager->HasUIWindowOpen()) return;
    if (!Controller) return;

    if (bIsAutoAttacking || bIsApproachingTarget)
    {
        StopAutoAttack();
    }

    const FRotator ControlYaw(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    const FVector Forward = ControlYaw.RotateVector(FVector::ForwardVector);
    AddMovementInput(Forward, MouseButtonsMoveForwardScale);

    DesiredMeshYaw = Controller->GetControlRotation().Yaw;
    bHasDesiredMeshYaw = true;
}

void ABasicPlayer::ClampControlPitch()
{
    if (!Controller) return;

    FRotator ControlRotation = Controller->GetControlRotation();
    ControlRotation.Pitch = FMath::ClampAngle(ControlRotation.Pitch, CameraPitchMin, CameraPitchMax);
    ControlRotation.Roll = 0.0f;
    Controller->SetControlRotation(ControlRotation);
}

void ABasicPlayer::UpdateCurrentPlayerMovement(float DeltaTime)
{
    FVector currentLocation = GetActorLocation();
    FRotator currentRotation = GetActorRotation();

    FVector newLocation = FVector(playerData.characterData.characterPosition.positionX,
        playerData.characterData.characterPosition.positionY,
        playerData.characterData.characterPosition.positionZ);

    // Always write the actual actor yaw so the server receives the correct facing direction.
    if (!bSimulateMovement)
    {
        playerData.characterData.characterPosition.rotationZ = currentRotation.Yaw;
    }

    // Compare current position and rotation to the last sent values
    bool hasPositionChanged = !currentLocation.Equals(LastSentPosition, PositionThreshold);
    // Bug 4 fix: also detect rotation-only changes (e.g. player turns to face a mob while
    // standing still) so remote clients see the correct facing direction in combat.
    bool hasRotationChanged = !FMath::IsNearlyEqual(currentRotation.Yaw, LastSentRotation.Yaw, 5.0f);

    TimeSinceLastUpdate = FMath::Min(TimeSinceLastUpdate + DeltaTime, UpdateInterval * 2.0f);

    if ((hasPositionChanged || hasRotationChanged) && TimeSinceLastUpdate >= UpdateInterval)
    {
        // Do not send movement packets while the player is dead
        if (playerData.characterData.bIsDead)
        {
            TimeSinceLastUpdate = 0.0f;
            return;
        }

        // Update player data with current state
        playerData.characterData.characterPosition.positionX = currentLocation.X;
        playerData.characterData.characterPosition.positionY = currentLocation.Y;
        playerData.characterData.characterPosition.positionZ = currentLocation.Z;

        // Send player movement to the game server
        MyGameInstance->PlayerManager->SendMovePlayerRequest(playerData);

        // Update the last sent position and rotation
        LastSentPosition = currentLocation;
        LastSentRotation = currentRotation;

        // Reset the timer
        TimeSinceLastUpdate = 0.0f;
    }
}


void ABasicPlayer::UpdateRemotePlayerMovement()
{
    const float DeltaTime = GetWorld()->GetDeltaSeconds();
    TimeSinceLastPositionUpdate += DeltaTime;

    // -----------------------------------------------------------------------
    // 1. Position interpolation
    //    Lerp from the position we were at when the packet arrived toward the
    //    server-authoritative target over exactly one server tick window.
    // -----------------------------------------------------------------------
    const float LerpWindow  = FMath::Max(ServerPositionUpdateInterval, 0.05f);
    const float LerpFactor  = FMath::Clamp(TimeSinceLastPositionUpdate / LerpWindow, 0.f, 1.f);
    const FVector NewPosition = FMath::Lerp(LastReceivedPosition, TargetReceivedPosition, LerpFactor);

    const float DistToTarget = FVector::Dist(GetActorLocation(), TargetReceivedPosition);

    if (DistToTarget > 1000.f)
    {
        // Hard-snap on large gaps (teleport / zone change).
        SetActorLocation(TargetReceivedPosition, false, nullptr, ETeleportType::TeleportPhysics);
    }
    else
    {
        SetActorLocation(NewPosition);
    }

    // -----------------------------------------------------------------------
    // 2. Rotation interpolation
    // -----------------------------------------------------------------------
    const FRotator NewRotation = FMath::RInterpTo(
        GetActorRotation(), TargetReceivedRotation, DeltaTime, 15.0f);
    SetActorRotation(NewRotation);

    // -----------------------------------------------------------------------
    // 3. Animation speed / direction вЂ” EMA smoothing + graceful stop fade-out
    //
    //    Problems solved:
    //      a) Raw RemoteSpeed jumps instantly from 0 в†’ full on first packet
    //         and stays at full until the 3x timeout в†’ causes abrupt start/stop.
    //      b) RemoteDirection flips instantly between packets в†’ blend-space pops.
    //
    //    Solution:
    //      вЂў Track idle time so we know when the server has stopped sending
    //        meaningful displacement packets.
    //      вЂў EMA-smooth speed toward the raw target value each tick.
    //      вЂў When idle, fade SmoothedRemoteSpeed to 0 with a short decay so the
    //        walkв†’idle transition in the anim graph is driven by a curve, not
    //        a hard zero.
    //      вЂў Direction is smoothed similarly so the blend-space never pops.
    // -----------------------------------------------------------------------

    // Grace period: one full server interval beyond the lerp window before we
    // consider the player idle.  This absorbs normal delivery jitter (~10-20 ms)
    // without introducing visible lag.
    const float GracePeriod = LerpWindow * 1.5f;

    if (bRemoteIsMoving)
    {
        RemoteIdleTime += DeltaTime;
        if (RemoteIdleTime >= GracePeriod)
        {
            // No displacement in the last packet вЂ” begin fade-out.
            bRemoteIsMoving = false;
        }
    }

    // Target values for this tick.
    const float TargetSpeed = bRemoteIsMoving ? RemoteSpeed : 0.0f;

    // EMA speeds: fast blend-in (0.2 weight on new) for startup,
    // slow blend-out (0.05 weight on new target=0) for graceful stop.
    // At 60 fps the fast path reaches ~98% of full speed in ~3 packets (~300 ms).
    // The slow path decays to near-zero in ~20 frames (~330 ms) вЂ” smooth fade.
    const float BlendIn  = FMath::Clamp(DeltaTime / 0.08f, 0.0f, 1.0f);  // ~80 ms rise
    const float BlendOut = FMath::Clamp(DeltaTime / 0.22f, 0.0f, 1.0f);  // ~220 ms fall
    const float SpeedAlpha = (TargetSpeed > SmoothedRemoteSpeed) ? BlendIn : BlendOut;

    SmoothedRemoteSpeed = FMath::Lerp(SmoothedRemoteSpeed, TargetSpeed, SpeedAlpha);

    // Snap to exactly zero to prevent the blend-space from hovering near 0
    // and flickering between idle and walk at very low smoothed values.
    // Also reset direction to 0 so the blend-space doesn't play a "slow backward"
    // idle when the player just stopped moving backward (Bug 2 fix).
    if (SmoothedRemoteSpeed < 2.0f)
    {
        SmoothedRemoteSpeed = 0.0f;
        SmoothedRemoteDirection = 0.0f;
    }

    // Direction: only blend when actually moving so we don't rotate the
    // blend-space axis while standing still (causes foot-sliding pop on restart).
    if (SmoothedRemoteSpeed > 2.0f)
    {
        const float DirAlpha = FMath::Clamp(DeltaTime / 0.10f, 0.0f, 1.0f);  // ~100 ms turn
        SmoothedRemoteDirection = FMath::Lerp(SmoothedRemoteDirection, RemoteDirection, DirAlpha);
    }
    // else: keep last known direction so the blend-space doesn't pop to 0 on stop.
}

    float ABasicPlayer::CalculateRotationInterpSpeed()
{
    float AngleDifference = FMath::Abs(LastReceivedRotation.Yaw - TargetReceivedRotation.Yaw);
    AngleDifference = FMath::Min(AngleDifference, 360.f - AngleDifference);
    return AngleDifference / ServerPositionUpdateInterval;
}


float ABasicPlayer::CalculateInterpolationSpeed(float MovementSpeed)
{
    float InterpolationSpeed = MovementSpeed * interpolationSpeedFactor;
    return FMath::Clamp(InterpolationSpeed, 1.0f, maxInterpolationSpeed);
}

FDateTime ABasicPlayer::StringToTimestamp(const FString& DateTimeString) {
    FDateTime DateTime;
    FDateTime::Parse(DateTimeString, DateTime);
    return DateTime;
}

// get player data is other client
bool ABasicPlayer::GetIsOtherClient()
{
	return playerData.isOtherClient;
}

//get is dead state
bool ABasicPlayer::GetIsDead() const
{
	return playerData.characterData.bIsDead;
}

// get is moving state
bool ABasicPlayer::GetIsMoving() const
{
    return playerData.characterData.bIsMoving;
}

// get current zone name
FString ABasicPlayer::GetCurrentZoneName()
{
	return CurrentZoneName;
}

// get player current HP points
int32 ABasicPlayer::GetPlayerCurrentHPPoints() const
{
    return playerData.characterData.characterCurrentHealth;
}

// get player current MP points
int32 ABasicPlayer::GetPlayerCurrentMPPoints() const
{
    return playerData.characterData.characterCurrentMana;
}

UPlayerAnimInstance* ABasicPlayer::GetPlayerAnimInstance() const
{
    return Cast<UPlayerAnimInstance>(GetMesh()->GetAnimInstance());
}

// Set message data
void ABasicPlayer::SetMessageData(const FMessageDataStruct NewMessageData)
{
	messageData = NewMessageData;
}

void ABasicPlayer::SetCurrentZoneName(const FString& NewZoneName)
{
    CurrentZoneName = NewZoneName;
}

// Set is other client
void ABasicPlayer::SetIsOtherClient(bool bIsOtherClient)
{
    playerData.isOtherClient = bIsOtherClient;
}

// Set client ID
void ABasicPlayer::SetClientID(int32 ID)
{
    playerData.clientId = ID;
}

void ABasicPlayer::SetPlayerTag(const FString& Tag)
{
    Tags.Add(FName(*Tag));
}

// Set client token
void ABasicPlayer::SetClientSecret(FString Secret)
{
    playerData.hash = Secret;
}

// Set character ID
void ABasicPlayer::SetCharacterID(int32 ID)
{
    playerData.characterData.characterId = ID;
}

// Set client login
void ABasicPlayer::SetClientLogin(FString Login)
{
    playerData.clientLogin = Login;
}

// set player class
void ABasicPlayer::SetPlayerClass(FString Class)
{
    playerData.characterData.characterClass = Class;
}

// set player race
void ABasicPlayer::SetPlayerRace(FString Race)
{
    playerData.characterData.characterRace = Race;
}

// set player name
void ABasicPlayer::SetPlayerName(FString Name)
{
    playerData.characterData.characterName = Name;
}

// set player level
void ABasicPlayer::SetPlayerLevel(int32 Level)
{
    playerData.characterData.characterLevel = Level;
	UpdateExperienceData(); // Update ExperienceManager when level changes
}

void ABasicPlayer::InitialiseNameplate(bool bIsLocal)
{
    if (NameplateComponent)
    {
        NameplateComponent->InitialiseFromCharacterData(playerData.characterData, bIsLocal, false);
    }
}

// set player experience points
void ABasicPlayer::SetPlayerExpPoints(int32 ExpPoints)
{
    playerData.characterData.characterExperiencePoints = ExpPoints;
	UpdateExperienceData(); // Update ExperienceManager when experience changes
}

// set player next level exp
void ABasicPlayer::SetPlayerNextLevelExp(int32 NextLevelExp)
{
	playerData.characterData.characterExpForLevelEnd = NextLevelExp;
	UpdateExperienceData(); // Update ExperienceManager when next level exp changes
}

// Update experience data in ExperienceManager
void ABasicPlayer::UpdateExperienceData()
{
	if (!MyGameInstance || playerData.isOtherClient)
	{
		return; // Only update for local player
	}

	UExperienceManager* ExperienceManager = MyGameInstance->GetExperienceManager();
	if (ExperienceManager && playerData.characterData.characterId > 0)
	{
		FPlayerProgressionStruct UpdatedProgression;
		UpdatedProgression.characterId       = playerData.characterData.characterId;
		UpdatedProgression.currentLevel      = playerData.characterData.characterLevel;
		UpdatedProgression.currentExperience = playerData.characterData.characterExperiencePoints;
		UpdatedProgression.totalExperience   = playerData.characterData.characterExperiencePoints;
		UpdatedProgression.expForNextLevel   = playerData.characterData.characterExpForLevelEnd;
		// characterExpForLevelStart is always kept current by UpdateCharacterDataFromStatsUpdate
		UpdatedProgression.expForCurrentLevel = playerData.characterData.characterExpForLevelStart;
		UpdatedProgression.experienceDebt    = playerData.characterData.characterExperienceDebt;
		UpdatedProgression.bHasPendingLevelUp = false;
		UpdatedProgression.pendingLevelGained = 0;

		ExperienceManager->UpdateCharacterProgression(playerData.characterData.characterId, UpdatedProgression);

		UE_LOG(LogTemp, Log, TEXT("Updated experience data for character %d: Level %d, XP %d [%d-%d]"), 
			playerData.characterData.characterId, 
			UpdatedProgression.currentLevel,
			UpdatedProgression.currentExperience,
			UpdatedProgression.expForCurrentLevel,
			UpdatedProgression.expForNextLevel);
	}
}

// set player current HP points
void ABasicPlayer::SetPlayerCurrentHPPoints(int32 CurrentHPPoints)
{
    playerData.characterData.characterCurrentHealth = CurrentHPPoints;

    if (MyGameInstance && !playerData.isOtherClient)
    {
        if (UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
        {
            FPlayerStatsUpdateStruct Updated = StatsMgr->GetCachedStats();
            if (Updated.characterId > 0)
            {
                Updated.healthCurrent = CurrentHPPoints;
                StatsMgr->ApplyStatsUpdate(Updated);
            }
        }
    }

    // Update HP bar on the nameplate for remote players
    if (playerData.isOtherClient && NameplateComponent)
    {
        const int32 MaxHP = GetMaxHealth_Implementation();
        NameplateComponent->UpdateHealth(CurrentHPPoints, MaxHP);
    }

    // Refresh local HUD immediately so HP bar updates in combat
    if (!playerData.isOtherClient)
    {
        RefreshHUD();
    }
}

// set player current MP points
void ABasicPlayer::SetPlayerCurrentMPPoints(int32 CurrentMPPoints)
{
    playerData.characterData.characterCurrentMana = CurrentMPPoints;

    if (MyGameInstance && !playerData.isOtherClient)
    {
        if (UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
        {
            FPlayerStatsUpdateStruct Updated = StatsMgr->GetCachedStats();
            if (Updated.characterId > 0)
            {
                Updated.manaCurrent = CurrentMPPoints;
                StatsMgr->ApplyStatsUpdate(Updated);
            }
        }
    }

    // Refresh local HUD immediately so MP bar updates in combat
    if (!playerData.isOtherClient)
    {
        RefreshHUD();
    }
}

// set player attributes
void ABasicPlayer::SetPlayerAttributes(TMap<FString, FAttributeDataStruct> Attributes)
{
	playerData.characterData.characterAttributes.attributesData = Attributes;

	//debug player attributes
	for (auto& Elem : Attributes)
	{
		FString Key = Elem.Key;
		FAttributeDataStruct Value = Elem.Value;
		UE_LOG(LogTemp, Warning, TEXT("Player Attribute Key: %s, Value: %d"), *Key, Value.attributeValue);
	}

}

// set player coordinates
void ABasicPlayer::SetCoordinates(double x, double y, double z, double rotZ)
{
    playerData.characterData.characterPosition.positionX = x;
    playerData.characterData.characterPosition.positionY = y;
    playerData.characterData.characterPosition.positionZ = z;
    playerData.characterData.characterPosition.rotationZ = rotZ;

    const FVector NewTarget(x, y, z);

    // First packet ever received: hard-snap the actor to the authoritative position
    // so it never interpolates from the uninitialized (0,0,0) origin.
    if (playerData.isOtherClient && !bHasReceivedFirstPosition)
    {
        bHasReceivedFirstPosition = true;
        SetActorLocation(NewTarget, false, nullptr, ETeleportType::TeleportPhysics);
        SetActorRotation(FRotator(0.0, rotZ, 0.0));
        LastReceivedPosition      = NewTarget;
        TargetReceivedPosition    = NewTarget;
        LastReceivedRotation      = FRotator(0.0, rotZ, 0.0);
        TargetReceivedRotation    = FRotator(0.0, rotZ, 0.0);
        TimeSinceLastPositionUpdate = 0.0f;
        return;
    }

    // Measure actual inter-packet delivery time and use it as the lerp window.
    // This makes interpolation robust to variable server tick rates and network jitter.
    if (playerData.isOtherClient && TimeSinceLastPositionUpdate > 0.01f)
    {
        // Clamp to a sane range (50ms вЂ“ 500ms) to ignore the very first packet
        // and any massive gaps caused by the player standing still.
        const float MeasuredInterval = FMath::Clamp(TimeSinceLastPositionUpdate, 0.05f, 0.5f);
        // Exponential moving average: 80% old value, 20% new measurement
        ServerPositionUpdateInterval = ServerPositionUpdateInterval * 0.8f + MeasuredInterval * 0.2f;
    }

    // Estimate 2D speed and direction from the XY displacement between consecutive positions.
    // RemoteSpeed / RemoteDirection are the raw instantaneous values.
    // Smoothing is applied every Tick inside UpdateRemotePlayerMovement.
    if (playerData.isOtherClient && ServerPositionUpdateInterval > 0.0f)
    {
        const float Dist2D = FVector::Dist2D(TargetReceivedPosition, NewTarget);

        // Large displacement (> 500 cm) almost certainly means a server-side teleport
        // (e.g. respawn, warp).  Hard-snap the actor and zero out all animation-speed
        // state so the character never plays a running animation in the new position
        // before the first real movement packet arrives.
        static constexpr float TeleportThreshold = 500.0f;
        if (Dist2D > TeleportThreshold)
        {
            SetActorLocation(NewTarget, false, nullptr, ETeleportType::TeleportPhysics);
            SetActorRotation(FRotator(0.0, rotZ, 0.0));
            LastReceivedPosition       = NewTarget;
            TargetReceivedPosition     = NewTarget;
            LastReceivedRotation       = FRotator(0.0, rotZ, 0.0);
            TargetReceivedRotation     = FRotator(0.0, rotZ, 0.0);
            RemoteSpeed                = 0.0f;
            SmoothedRemoteSpeed        = 0.0f;
            RemoteDirection            = 0.0f;
            bRemoteIsMoving            = false;
            RemoteIdleTime             = 0.0f;
            TimeSinceLastPositionUpdate = 0.0f;
            return;
        }

        RemoteSpeed = Dist2D / ServerPositionUpdateInterval;

        if (Dist2D > 1.0f)
        {
            bRemoteIsMoving = true;
            RemoteIdleTime  = 0.0f;

            const FVector Delta2D = FVector(NewTarget.X - TargetReceivedPosition.X,
                                            NewTarget.Y - TargetReceivedPosition.Y, 0.0f).GetSafeNormal();
            const FVector ActorForward = GetActorForwardVector();
            const FVector ActorRight   = GetActorRightVector();
            RemoteDirection = FMath::RadiansToDegrees(
                FMath::Atan2(FVector::DotProduct(Delta2D, ActorRight),
                             FVector::DotProduct(Delta2D, ActorForward)));
        }
        else
        {
            // Packet arrived but player hasn't moved вЂ” treat as idle.
            RemoteSpeed     = 0.0f;
            RemoteDirection = 0.0f;
        }
    }

    LastReceivedPosition = GetActorLocation();
    TargetReceivedPosition = NewTarget;

    LastReceivedRotation = GetActorRotation();
    TargetReceivedRotation = FRotator(0, rotZ, 0);

    TimeSinceLastPositionUpdate = 0.0f;
}



// Play sound
void ABasicPlayer::PlaySound(USoundBase* Sound)
{
    // Always re-stamp the SoundClass override so this component stays under
    // AudioManager volume control even if it was constructed before the
    // GameInstance had a valid SFXClass reference.
    if (MyGameInstance && MyGameInstance->AudioManager && MyGameInstance->AudioManager->SFXClass)
    {
        AudioComponent->SoundClassOverride = MyGameInstance->AudioManager->SFXClass;
    }
    AudioComponent->SetSound(Sound);
    AudioComponent->Play();
}

// Stop sound
void ABasicPlayer::StopSound()
{
    AudioComponent->Stop();
}

const FEntityAudioProfile* ABasicPlayer::GetAudioProfile() const
{
    if (AudioProfileId.IsNone()) return nullptr;
    if (!MyGameInstance) return nullptr;
    if (UEntityAudioRepository* Repo = MyGameInstance->GetEntityAudioRepository())
    {
        return Repo->FindProfile(AudioProfileId);
    }
    return nullptr;
}

void ABasicPlayer::PlayEventSound(const TSoftObjectPtr<USoundBase>& SoundRef)
{
    if (SoundRef.IsNull()) return;
    USoundBase* Sound = SoundRef.LoadSynchronous();
    if (!Sound) return;

    USoundClass* SFXClass = (MyGameInstance && MyGameInstance->AudioManager)
        ? MyGameInstance->AudioManager->SFXClass : nullptr;

    if (SFXClass)
    {
        UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
            Sound,
            GetRootComponent(),
            NAME_None,
            GetActorLocation(),
            FRotator::ZeroRotator,
            EAttachLocation::KeepWorldPosition,
            /*bStopWhenAttachedToDestroyed=*/true,
            /*VolumeMultiplier=*/1.0f,
            /*PitchMultiplier=*/1.0f,
            /*StartTime=*/0.0f,
            /*AttenuationSettings=*/nullptr,
            /*ConcurrencySettings=*/nullptr,
            /*bAutoActivate=*/false);
        if (AC)
        {
            AC->SoundClassOverride = SFXClass;
            AC->bAutoDestroy = true;
            AC->Play();
        }
    }
    else
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
    }
}

void ABasicPlayer::StartMovementSimulation()
{
    bSimulateMovement = true;
}

void ABasicPlayer::StopMovementSimulation()
{
    bSimulateMovement = false;
}

void ABasicPlayer::UpdateMovementSimulation(float DeltaTime)
{
    FVector CurrentLocation = GetActorLocation();
    float DistanceToTarget = FVector::Dist(CurrentLocation, TargetPosition);

    if (DistanceToTarget < 50.f) // Threshold to decide when to pick a new target
    {
        float NewX = FMath::FRandRange(SquareCenter.X - SideLength / 2, SquareCenter.X + SideLength / 2);
        float NewY = FMath::FRandRange(SquareCenter.Y - SideLength / 2, SquareCenter.Y + SideLength / 2);
        TargetPosition = FVector(NewX, NewY, 90.0f); // Assuming Z is constant for this example

        FVector MovementDirection = (TargetPosition - CurrentLocation).GetSafeNormal();
        if (!MovementDirection.IsNearlyZero())
        {
            // Calculate the desired rotation based on the movement direction
            FRotator DesiredRotation = MovementDirection.Rotation();
            DesiredRotation.Pitch = 0.0f; // Keep the pitch level, adjust if your game needs vertical aiming
            DesiredRotation.Roll = 0.0f;  // Typically, you don't need to roll the character

            playerData.characterData.characterPosition.rotationZ = DesiredRotation.Yaw;
        }


        // Logging for debugging
        UE_LOG(LogTemp, Warning, TEXT("New target position: %s"), *TargetPosition.ToString());
    }

    // Move towards TargetPosition if not already close
    if (DistanceToTarget > 1.0f) // Use a small threshold to avoid jittering at the target location
    {
        FVector Direction = (TargetPosition - CurrentLocation).GetSafeNormal();
        float MovementStep = MoveSpeed * 2 * DeltaTime; // Adjust as needed
        //float MovementStep = MoveSpeed* GetWorld()->GetDeltaSeconds();
        FVector NewPosition = CurrentLocation + Direction * MovementStep;
        SetActorLocation(NewPosition);
    }
}

void ABasicPlayer::OnQuestJournalToggle()
{
    if (playerData.isOtherClient)
    {
        return;
    }

    if (UIManager)
    {
        UIManager->ToggleQuestJournal();
    }
}

void ABasicPlayer::OnEquipmentToggle()
{
    if (playerData.isOtherClient)
    {
        return;
    }

    if (UIManager)
    {
        UIManager->ToggleEquipment();
    }
}

void ABasicPlayer::OnAltCursorToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleAltCursor();
}

void ABasicPlayer::OnStatsToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->TogglePlayerStats();
}

void ABasicPlayer::OnBestiaryToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleBestiary();
}

void ABasicPlayer::OnTitlesToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleTitles();
}

void ABasicPlayer::OnReputationToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleReputation();
}

void ABasicPlayer::OnEmoteListToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleEmoteList();
}

void ABasicPlayer::CheckForNPC()
{
    if (playerData.isOtherClient)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        return;
    }

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
    const FVector CameraForward = CameraRotation.Vector();

    // First try a line trace with ECC_Pawn to hit NPC capsules directly
    const float TraceDistance = 1500.0f;
    FVector TraceEnd = CameraLocation + CameraForward * TraceDistance;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    FHitResult Hit;
    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Pawn, Params);
    ABasicNPC* HitNPC = bHit ? Cast<ABasicNPC>(Hit.GetActor()) : nullptr;

    // Fallback: find the NPC closest to the camera crosshair within range
    if (!HitNPC)
    {
        TArray<AActor*> AllNPCs;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABasicNPC::StaticClass(), AllNPCs);

        ABasicNPC* BestNPC = nullptr;
        float BestDot = 0.97f; // ~14 degree cone around crosshair

        for (AActor* Actor : AllNPCs)
        {
            ABasicNPC* NPC = Cast<ABasicNPC>(Actor);
            if (!NPC) continue;

            const float Dist = FVector::Dist(CameraLocation, NPC->GetActorLocation());
            if (Dist > TraceDistance) continue;

            const FVector ToNPC = (NPC->GetActorLocation() - CameraLocation).GetSafeNormal();
            const float Dot = FVector::DotProduct(CameraForward, ToNPC);
            if (Dot > BestDot)
            {
                // LOS check: is the NPC actually visible through world geometry?
                FHitResult LOSHit;
                FCollisionQueryParams LOSParams;
                LOSParams.AddIgnoredActor(this);
                LOSParams.AddIgnoredActor(NPC);
                const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
                    LOSHit, CameraLocation, NPC->GetActorLocation(), ECC_Visibility, LOSParams);
                if (!bBlocked)
                {
                    BestDot = Dot;
                    BestNPC = NPC;
                }
            }
        }
        HitNPC = BestNPC;
    }

    if (HitNPC && !HitNPC->IsNPCInteractable())
    {
        UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: CheckForNPC hit NPC '%s' but IsNPCInteractable() is false"), *HitNPC->GetNPCName());
    }

    TrackedNPC = (HitNPC && HitNPC->IsNPCInteractable()) ? HitNPC : nullptr;
}

void ABasicPlayer::CheckNPCInteractionDistance()
{
    if (!MyGameInstance || !UIManager) return;

    const int32 ActiveNpcId = UIManager->GetActiveInteractionNpcId();
    if (ActiveNpcId == 0) return; // No NPC windows are open.

    UNPCManager* NPCMgr = MyGameInstance->GetNPCManager();
    if (!NPCMgr) return;

    ABasicNPC* NPC = NPCMgr->GetNPCById(ActiveNpcId);
    if (!IsValid(NPC)) return;

    // Use 2D distance (ignores Z) so hills/ramps don't accidentally trigger close.
    const float Dist2D = FVector::Dist2D(GetActorLocation(), NPC->GetActorLocation());

    // Close threshold: interaction radius + 300 units so the player has to clearly
    // walk away before windows auto-close. This must exceed npc.radius (200 default).
    const float CloseThreshold = static_cast<float>(NPC->GetNPCData().radius) + 300.0f;

    if (Dist2D > CloseThreshold)
    {
        UE_LOG(LogTemp, Log, TEXT("[NPC] Player moved %.0f units from NPC %d (threshold %.0f). Force-closing NPC windows."),
            Dist2D, ActiveNpcId, CloseThreshold);
        UDialogueManager* DlgMgr = MyGameInstance->GetDialogueManager();
        UIManager->ForceCloseAllNPCWindows(DlgMgr);
    }
}

void ABasicPlayer::OnInteractInput()
{
    UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: OnInteractInput triggered"));

    if (playerData.characterData.bIsDead) return;

    if (playerData.isOtherClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: OnInteractInput skipped - isOtherClient"));
        return;
    }

    if (!TrackedNPC)
    {
        // No NPC in range — try World Interactive Object instead
        TryInteractWithWIO();
        return;
    }
    if (!MyGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("BasicPlayer: OnInteractInput - MyGameInstance is NULL"));
        return;
    }

    UDialogueManager* DlgManager = MyGameInstance->GetDialogueManager();
    if (!DlgManager)
    {
        UE_LOG(LogTemp, Error, TEXT("BasicPlayer: DialogueManager not found"));
        return;
    }

    if (DlgManager->IsDialogueActive())
    {
        // Close running session (and any open shop windows) before opening a new one.
        UIManager ? UIManager->ForceCloseAllNPCWindows(DlgManager) : DlgManager->CloseDialogue();
        return;
    }

    // If a shop from a DIFFERENT NPC is still open, close it before starting new dialogue.
    if (UIManager)
    {
        const int32 ActiveNpcId = UIManager->GetActiveInteractionNpcId();
        if (ActiveNpcId > 0 && ActiveNpcId != TrackedNPC->GetNPCId())
        {
            UE_LOG(LogTemp, Log, TEXT("[NPC] Closing windows for NPC %d before opening dialogue with NPC %d"),
                ActiveNpcId, TrackedNPC->GetNPCId());
            UIManager->ForceCloseAllNPCWindows(DlgManager);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Opening dialogue with NPC %d (%s)"),
        TrackedNPC->GetNPCId(), *TrackedNPC->GetNPCName());

    DlgManager->OpenDialogue(TrackedNPC->GetNPCId());
}

void ABasicPlayer::CheckForMOB()
{
    if (playerData.isOtherClient) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    // Trace from the camera viewpoint пїЅ consistent with CheckForNPC
    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
    const FVector CameraForward = CameraRotation.Vector();

    const float TraceDistance = 1500.0f;
    const FVector TraceEnd = CameraLocation + CameraForward * TraceDistance;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    ABasicMOB* SoftTarget = nullptr;

    // Primary: direct line trace against pawn capsules
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Pawn, Params))
    {
        ABasicMOB* Mob = Cast<ABasicMOB>(Hit.GetActor());
        if (Mob && !Mob->GetMOBIsDead())
        {
            SoftTarget = Mob;
        }
    }

    // Fallback: dot-product cone search (handles meshes that block ECC_Pawn poorly)
    if (!SoftTarget)
    {
        float BestDot = 0.97f; // ~14 degree cone around crosshair
        TArray<AActor*> AllMOBs;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABasicMOB::StaticClass(), AllMOBs);
        for (AActor* Actor : AllMOBs)
        {
            ABasicMOB* Mob = Cast<ABasicMOB>(Actor);
            if (!Mob || Mob->GetMOBIsDead()) continue;

            const float Dist = FVector::Dist(CameraLocation, Mob->GetActorLocation());
            if (Dist > TraceDistance) continue;

            const FVector ToMob = (Mob->GetActorLocation() - CameraLocation).GetSafeNormal();
            const float Dot = FVector::DotProduct(CameraForward, ToMob);
            if (Dot > BestDot)
            {
                // LOS check: is the mob actually visible through world geometry?
                FHitResult LOSHit;
                FCollisionQueryParams LOSParams;
                LOSParams.AddIgnoredActor(this);
                LOSParams.AddIgnoredActor(Mob);
                const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
                    LOSHit, CameraLocation, Mob->GetActorLocation(), ECC_Visibility, LOSParams);
                if (!bBlocked)
                {
                    BestDot = Dot;
                    SoftTarget = Mob;
                }
            }
        }
    }

    // Auto-clear locked target when mob dies
    if (LockedTarget && LockedTarget->GetMOBIsDead())
    {
        ClearLockedTarget();
    }

    // Auto-clear locked target when mob enters RETURNING state (deaggro / returning to spawn zone)
    if (LockedTarget && LockedTarget->MOBMovementComponent)
    {
        const int32 MobCombatState = LockedTarget->MOBMovementComponent->GetCombatState();
        if (MobCombatState == 5) // RETURNING
        {
            ClearLockedTarget();
        }
    }

    // Auto-clear locked target when player runs too far away (leash)
    const float MaxLeashDistance = 3000.0f;
    if (LockedTarget && !bIsAutoAttacking && !bIsApproachingTarget)
    {
        if (FVector::Dist(GetActorLocation(), LockedTarget->GetActorLocation()) > MaxLeashDistance)
        {
            ClearLockedTarget();
        }
    }

    // Effective target: hard lock takes priority over soft target
    ABasicMOB* EffectiveTarget = LockedTarget ? LockedTarget : SoftTarget;

    // Update skill system target
    const int32 EffectiveTargetId = EffectiveTarget
        ? FCString::Atoi(*EffectiveTarget->GetMOBUId()) : 0;

    if (UIManager)
    {
        UIManager->SetSkillTarget(EffectiveTargetId,
            EffectiveTargetId > 0 ? ECasterType::Mob : ECasterType::None);

        if (LockedTarget)
        {
            const int32 MaxHP = LockedTarget->GetMOBAttributes().attributesData.Contains(TEXT("max_health"))
                ? LockedTarget->GetMOBAttributes().attributesData[TEXT("max_health")].attributeValue
                : 100;
            UIManager->UpdateMobTargetFrameHP(LockedTarget->GetMOBCurrentHealth(), MaxHP);
        }
    }

    // Show/hide head widget using a cached pointer пїЅ no TActorIterator every Tick
    if (PrevSoftTarget != EffectiveTarget)
    {
        // Hide the widget on the mob that just left the crosshair
        // (skip if it is the hard-locked target пїЅ SetLockedTarget already manages it)
        if (PrevSoftTarget && PrevSoftTarget != LockedTarget && IsValid(PrevSoftTarget))
        {
            PrevSoftTarget->MobHeadInfo->ShowWidget(false);
        }

        // Show the widget on the new effective target
        if (EffectiveTarget && IsValid(EffectiveTarget))
        {
            EffectiveTarget->MobHeadInfo->ShowWidget(true);
        }

        PrevSoftTarget = EffectiveTarget;
    }
}


void ABasicPlayer::AttackTarget(int32 TargetID, const FString& SkillSlug, int32 TargetTypeId)
{
    if (!MyGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot attack: MyGameInstance not found"));
        return;
    }

    // Get combat system manager from game instance
    UCombatSystemManager* CombatManager = MyGameInstance->GetCombatSystemManager();
    if (!CombatManager)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot attack: CombatSystemManager not found"));
        return;
    }

    // Get skill system manager
    USkillSystemManager* SkillManager = MyGameInstance->GetSkillSystemManager();
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot attack: SkillSystemManager not found"));
        return;
    }

    if (TargetID <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot attack: Invalid target ID"));
        return;
    }

    // Check if we can cast the skill
    if (!SkillManager->CanCastSkill(GetActorId_Implementation(), SkillSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot cast skill %s"), *SkillSlug);
        return;
    }

    // Convert TargetTypeId to ECasterType using real server protocol: PLAYER=2, MOB=3
    ECasterType TargetType = ECasterType::Mob; // Default
    switch (TargetTypeId)
    {
        case 2:
            TargetType = ECasterType::Player;
            break;
        case 3:
            TargetType = ECasterType::Mob;
            break;
        default:
            TargetType = ECasterType::Mob;
            break;
    }

    // Use skill system to cast the skill
    if (SkillManager->CastSkill(GetActorId_Implementation(), TargetID, SkillSlug, TargetType))
    {
        UE_LOG(LogTemp, Warning, TEXT("Player %d attacking target ID: %d with skill: %s, target type id: %d"), 
            GetActorId_Implementation(), TargetID, *SkillSlug, TargetTypeId);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to cast skill %s on target %d"), *SkillSlug, TargetID);
    }
}

// Called when the actor is being destroyed
void ABasicPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear any pending timers to avoid use-after-free in lambdas
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(UIInitTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(HitPointTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(CastBarTimerHandle);
    }

    // For remote players: remove the equipment visual delegate binding.
    // This is a safety net вЂ” RemovePlayerData() does it first, but if the actor
    // is destroyed by other means (e.g. level unload) this prevents dangling callbacks.
    if (playerData.isOtherClient && MyGameInstance && EquipmentVisualComponent)
    {
        if (UEquipmentManager* EqMgr = MyGameInstance->GetEquipmentManager())
        {
            EqMgr->OnRemoteEquipmentStateReceivedDelegate.RemoveDynamic(
                EquipmentVisualComponent, &UEquipmentVisualComponent::HandleRemoteEquipmentState);
        }
    }

    // Unregister from actor registry
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        const int32 ActorId = GetActorId_Implementation();
        if (GameInstance->MOBManager && ActorId > 0)
        {
            GameInstance->MOBManager->UnregisterPlayer(ActorId);
        }
    }

    // Unregister from combat system
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (UCombatSystemManager* CombatManager = GameInstance->GetCombatSystemManager())
        {
            if (IsValid(this) && GetActorId_Implementation() > 0)
            {
                TScriptInterface<ICombatable> CombatableInterface;
                CombatableInterface.SetObject(this);
                CombatableInterface.SetInterface(this);
                
                CombatManager->UnregisterCombatable(CombatableInterface);
                UE_LOG(LogTemp, Log, TEXT("Player %d unregistered from combat system"), GetActorId_Implementation());
            }
        }
    }

    // Unsubscribe from PlayerStatsManager to prevent dangling delegate callbacks.
    if (!playerData.isOtherClient && MyGameInstance)
    {
        if (UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
        {
            StatsMgr->OnStatsUpdated.RemoveDynamic(this, &ABasicPlayer::HandleStatsManagerUpdate);
        }
    }

    Super::EndPlay(EndPlayReason);
}

// ICombatable interface implementations
int32 ABasicPlayer::GetMaxHealth_Implementation() const
{
    if (const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health")))
    {
        if (HealthAttr->attributeValue > 0)
        {
            return HealthAttr->attributeValue;
        }
    }
    // Fallback: if max_health not yet received from stats_update, return current HP
    // so percentage calculations don't produce absurd values (e.g. 435/1).
    return FMath::Max(1, playerData.characterData.characterCurrentHealth);
}

int32 ABasicPlayer::GetMaxMana_Implementation() const
{
    if (const FAttributeDataStruct* ManaAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana")))
    {
        if (ManaAttr->attributeValue > 0)
        {
            return ManaAttr->attributeValue;
        }
    }
    return FMath::Max(1, playerData.characterData.characterCurrentMana);
}

void ABasicPlayer::SetDead_Implementation(bool bNewDead)
{
    playerData.characterData.bIsDead = bNewDead;

    // Sync dead state on the nameplate (works for both local and remote пїЅ local is already hidden)
    if (NameplateComponent)
    {
        NameplateComponent->SetDeadState(bNewDead);
    }

    if (bNewDead)
    {
        UE_LOG(LogTemp, Warning, TEXT("Player %d has died"), GetActorId_Implementation());
        OnDeath_Implementation();
    }
    else
    {
        // Revive: clear any lingering combat / interaction state (#6)
        bIsPickingUp = false;
        bIsCasting   = false;
        bIsApproachingTarget = false;
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);
        }

        // Restore movement so the player can walk again
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->SetMovementMode(MOVE_Walking);
        }
        HideDeathScreen();
        // Play revive sound
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            PlayEventSound(Profile->Revive);
        }
        // Notify Blueprint AnimBP so it can exit the death state
        OnRevive();
        UE_LOG(LogTemp, Warning, TEXT("Player %d has been revived"), GetActorId_Implementation());
    }
}

void ABasicPlayer::OnDeath_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("Player %s died"), *playerData.characterData.characterName);

    // Stop any ongoing auto-attack / approach cycle (#5)
    StopAutoAttack();
    ClearLockedTarget();
    ClearTarget_Implementation();

    // Release pickup/cast locks so they cannot block input after respawn
    bIsPickingUp = false;
    bIsCasting   = false;

    // Play death sound
    if (const FEntityAudioProfile* Profile = GetAudioProfile())
    {
        PlayEventSound(Profile->Death);
    }

    // Disable movement so the player can't walk while dead
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->DisableMovement();
    }

    // Release any held mouse buttons so the corpse cannot be rotated.
    // The input events fire while the death screen is fading in and bIsDead
    // is already true, so Look() guards against any stray events as well.
    bIsRightMouseDown = false;
    bIsLeftMouseDown  = false;
    RestoreCursorToUIManager();

    // Drive the AnimBP death state
    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        AnimInst->NotifyDeath();
    }

    // Show the death screen overlay
    ShowDeathScreen();
}

void ABasicPlayer::OnRevive()
{
    UE_LOG(LogTemp, Log, TEXT("Player %d OnRevive"), GetActorId_Implementation());

    // Notify AnimBP so it can transition out of the death state
    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        AnimInst->NotifyRevive();
    }
}

void ABasicPlayer::SetTarget_Implementation(int32 TargetId, ECasterType TargetType)
{
    CurrentTargetId = TargetId;
    CurrentTargetType = TargetType;
    
    UE_LOG(LogTemp, Log, TEXT("Player %d set target: %d (%s)"), 
        GetActorId_Implementation(), TargetId, *UEnum::GetValueAsString(TargetType));
}

void ABasicPlayer::ClearTarget_Implementation()
{
    CurrentTargetId = 0;
    CurrentTargetType = ECasterType::None;
    
    UE_LOG(LogTemp, Log, TEXT("Player %d cleared target"), GetActorId_Implementation());
}

void ABasicPlayer::PlaySkillAnimation_Implementation(const FString& AnimationName, const FString& SkillSlug, float Duration)
{
    UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] PlaySkillAnimation: player=%d anim='%s' slug='%s' duration=%.3fs"),
        GetActorId_Implementation(), *AnimationName, *SkillSlug, Duration);

    if (!MyGameInstance) return;
    UCombatSystemManager* CombatMgr = MyGameInstance->GetCombatSystemManager();
    if (!CombatMgr) return;

    // Track the animating skill slug for audio/VFX only (see PlayCombatSoundEvent).
    // CurrentSkillName is intentionally NOT overwritten here so the auto-attack
    // loop (DoAutoAttack) always sends "basic_attack" to the server.
    ActiveAnimSkillSlug = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;
    CurrentAnimationDuration = Duration;

    const int32 CasterId = GetActorId_Implementation();

    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] AnimInstance found (%s), calling StartAttack"),
            *AnimInst->GetClass()->GetName());

        // Remove any previous OnHitPoint binding so we never fire twice
        if (HitPointDelegateHandle.IsValid())
        {
            AnimInst->OnHitPoint.Remove(HitPointDelegateHandle);
            HitPointDelegateHandle.Reset();
        }

        // For projectile skills the hit-point is notified by the projectile on impact,
        // NOT by the animation notify. Only bind the anim-notify path for melee/instant skills.
        const FString LookupKeyEarly = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;
        bool bSkillHasProjectile = false;
        if (MyGameInstance)
        {
            if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
            {
                bSkillHasProjectile = !Repo->GetDefinition(LookupKeyEarly).projectileClass.IsNull();
            }
        }

        if (!bSkillHasProjectile)
        {
            HitPointDelegateHandle = AnimInst->OnHitPoint.AddLambda([CombatMgr](int32 InCasterId)
            {
                if (IsValid(CombatMgr))
                {
                    CombatMgr->NotifyHitPoint(InCasterId);
                }
            });
        }

        // Bind OnAttackEnded > PlayerSkillManager::NotifyAnimationEnded so the cast
        // lock is released only after the montage fully completes, preventing spam.
        // Bug 7 fix: also remove any stale AutoAttackAnimEndDelegateHandle so that a
        // lingering auto-attack end binding cannot trigger ClearLockedTarget() when
        // a server-confirmed skill animation ends while bIsAutoAttacking=false.
        if (AutoAttackAnimEndDelegateHandle.IsValid())
        {
            AnimInst->OnAttackEnded.Remove(AutoAttackAnimEndDelegateHandle);
            AutoAttackAnimEndDelegateHandle.Reset();
        }
        if (AnimEndDelegateHandle.IsValid())
        {
            AnimInst->OnAttackEnded.Remove(AnimEndDelegateHandle);
            AnimEndDelegateHandle.Reset();
        }
        if (UPlayerSkillManager* SkillMgr = MyGameInstance ? MyGameInstance->GetPlayerSkillManager() : nullptr)
        {
            AnimEndDelegateHandle = AnimInst->OnAttackEnded.AddUObject(
                SkillMgr, &UPlayerSkillManager::NotifyAnimationEnded);
        }

        FSkillInitiationData SkillData;
        SkillData.animationName      = AnimationName;
        SkillData.animationDuration  = Duration;
        SkillData.casterId           = CasterId;
        SkillData.castTime           = CurrentCastTime; // set by ShowCastBar_Implementation before this call

        // Pre-compute swing time now, while CurrentCastTime is still valid.
        // HideCastBar resets CurrentCastTime at T=castTime; the CastRelease notify
        // fires one frame later, so reading CurrentCastTime there gives 0 (wrong speed).
        CurrentSwingSeconds = (CurrentCastTime > 0.001f)
            ? FMath::Max(Duration - CurrentCastTime, 0.05f)
            : Duration;

        AnimInst->StartAttack(SkillData);

        // --- Cast sound & projectile from SkillDefinitionRepository ---
        if (MyGameInstance)
        {
            if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
            {
                // Use SkillSlug for definition lookup; fall back to AnimationName so older
                // DataTable rows that still use animationName as their key still work.
                const FString LookupKey = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;
                const FSkillDefinitionData& Def = Repo->GetDefinition(LookupKey);

                // Play cast sound at player location
                if (!Def.castSound.IsNull())
                {
                    if (USoundBase* Sound = Def.castSound.LoadSynchronous())
                    {
                        SpawnSFXAttached(this, Sound, GetActorLocation());
                    }
                }

                // Niagara cast effect
                if (!Def.castEffectNiagara.IsNull())
                {
                    if (UNiagaraSystem* NiagaraEffect = Def.castEffectNiagara.LoadSynchronous())
                    {
                        FVector CastLoc = GetActorLocation();
                        FRotator CastRot = GetActorRotation();
                        if (Def.CastSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.CastSocketName))
                        {
                            CastLoc = GetMesh()->GetSocketLocation(Def.CastSocketName);
                            CastRot = GetMesh()->GetSocketRotation(Def.CastSocketName);
                        }
                        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                            GetWorld(), NiagaraEffect, CastLoc, CastRot);
                    }
                }

                // swingSound and VoiceAttack are now fired via AnimNotify_PlayerCombatEvent
                // placed on the montage at the correct timing frame.
                // Projectile is now spawned at AnimNotify CastRelease via PlayCombatSoundEvent.
            }
        }
    }
    else
    {
        // AnimBP parent class is not UPlayerAnimInstance пїЅ log the actual class so we can fix it
        if (GetMesh() && GetMesh()->GetAnimInstance())
        {
            UE_LOG(LogTemp, Error,
                TEXT("[PlayerAnim] GetPlayerAnimInstance() returned nullptr! "
                     "Actual AnimInstance class: %s. "
                     "Set Anim Class parent to UPlayerAnimInstance in the Anim BP."),
                *AnimInst->GetClass()->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("[PlayerAnim] GetPlayerAnimInstance() returned nullptr and no AnimInstance exists on mesh!"));
        }

        // Fallback: no AnimInstance assigned yet пїЅ schedule timer directly
        const float HitDelay = FMath::Max(Duration * 0.45f, 0.05f);
        UE_LOG(LogTemp, Warning, TEXT("[PlayerAnim] FALLBACK timer: HitDelay=%.3fs"), HitDelay);
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(HitPointTimerHandle);
            World->GetTimerManager().SetTimer(HitPointTimerHandle, [CombatMgr, CasterId]()
            {
                if (IsValid(CombatMgr))
                {
                    CombatMgr->NotifyHitPoint(CasterId);
                }
            }, HitDelay, false);
        }
    }
}

void ABasicPlayer::ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School, bool bIsMissed, bool bIsBlocked, const FString& SkillSlug)
{
    UE_LOG(LogTemp, Log, TEXT("Player %d taking %d damage (Critical: %s, School: %s, Missed: %s, Blocked: %s, Skill: %s)"),
        GetActorId_Implementation(), Damage,
        bIsCritical ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(School),
        bIsMissed ? TEXT("true") : TEXT("false"),
        bIsBlocked ? TEXT("true") : TEXT("false"),
        *SkillSlug);

    // Trigger hit-react animation вЂ” only on actual hit, not miss
    if (!bIsMissed)
    {
        if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
        {
            AnimInst->NotifyHit();
        }

        // Interrupt emote on damage
        if (EmoteComponent)
        {
            EmoteComponent->NotifyDamageReceived();
        }
    }

    // Play hit received sound (generic player grunt / armor clank)
    if (!bIsMissed)
    {
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            PlayEventSound(Profile->HitReceived);
        }
    }

    // --- Hit sound + hit particle from the skill that caused the damage ---
    bool bHitSoundPlayed = false;
    if (MyGameInstance)
    {
        if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

            // --- Impact sound: WeaponImpactType ? ArmorMaterialType lookup ---
            // ArmorMaterialType is read from the chest slot item in DT_ItemVisuals.
            if (Def.WeaponImpactType != NAME_None)
            {
                if (UDataTable* ImpactTable = MyGameInstance->GetImpactSoundsTable())
                {
                    // Resolve ArmorMaterialType from the equipped chest piece via DT_ItemVisuals
                    FName ArmorMat = NAME_None;
                    if (UEquipmentManager* EqMgr = MyGameInstance->GetEquipmentManager())
                    {
                        FEquipmentSlotData ChestSlot = EqMgr->GetSlot(TEXT("chest"));
                        if (ChestSlot.bIsOccupied && !ChestSlot.itemSlug.IsEmpty())
                        {
                            if (UDataTable* VisualsTable = MyGameInstance->GetItemVisualsDataTable())
                            {
                                FName VisualKey = FName(*ChestSlot.itemSlug);
                                if (const FItemVisualData* VisRow = VisualsTable->FindRow<FItemVisualData>(VisualKey, TEXT("")))
                                {
                                    ArmorMat = VisRow->ArmorMaterialType;
                                }
                            }
                        }
                    }

                    // Fallback: no chest armor = flesh
                    if (ArmorMat == NAME_None)
                    {
                        ArmorMat = FName(TEXT("flesh"));
                    }

                    FName ImpactKey = FName(*FString::Printf(TEXT("%s_%s"),
                        *Def.WeaponImpactType.ToString(), *ArmorMat.ToString()));

                    if (const FImpactSoundData* ImpactRow = ImpactTable->FindRow<FImpactSoundData>(ImpactKey, TEXT("")))
                    {
                        if (ImpactRow->ImpactSounds.Num() > 0)
                        {
                            int32 Idx = FMath::RandRange(0, ImpactRow->ImpactSounds.Num() - 1);
                            if (USoundBase* ImpactSound = ImpactRow->ImpactSounds[Idx].LoadSynchronous())
                            {
                                SpawnSFXAttached(this, ImpactSound, GetActorLocation());
                                bHitSoundPlayed = true;
                            }
                        }

                        // Spawn impact VFX if defined
                        if (!ImpactRow->ImpactVFX.IsNull())
                        {
                            if (UNiagaraSystem* ImpactVFX = ImpactRow->ImpactVFX.LoadSynchronous())
                            {
                                FVector HitLoc = GetCombatPosition_Implementation();
                                if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
                                {
                                    HitLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
                                }
                                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, HitLoc);
                            }
                        }
                    }
                }
            }

            // Fallback: generic hitSound from skill definition
            if (!bHitSoundPlayed && !Def.hitSound.IsNull())
            {
                if (USoundBase* Sound = Def.hitSound.LoadSynchronous())
                {
                    UAudioComponent* AC = UGameplayStatics::SpawnSoundAtLocation(this, Sound, GetActorLocation());
                    if (AC && MyGameInstance && MyGameInstance->AudioManager && MyGameInstance->AudioManager->SFXClass)
                    {
                        AC->SoundClassOverride = MyGameInstance->AudioManager->SFXClass;
                    }
                }
            }

            // Niagara hit effect
            if (!Def.hitEffectNiagara.IsNull())
            {
                if (UNiagaraSystem* NiagaraEffect = Def.hitEffectNiagara.LoadSynchronous())
                {
                    FVector HitLoc = GetCombatPosition_Implementation();
                    FRotator HitRot = FRotator::ZeroRotator;
                    if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
                    {
                        HitLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
                        HitRot = GetMesh()->GetSocketRotation(Def.HitSocketName);
                    }
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                        GetWorld(), NiagaraEffect, HitLoc, HitRot);
                }
            }

            // Critical hit sound (layered on top of regular hit sound)
            if (bIsCritical && !bIsMissed && !Def.critSound.IsNull())
            {
                if (USoundBase* CritSnd = Def.critSound.LoadSynchronous())
                {
                    UAudioComponent* CritAC = UGameplayStatics::SpawnSoundAtLocation(this, CritSnd, GetActorLocation());
                    if (CritAC && MyGameInstance && MyGameInstance->AudioManager && MyGameInstance->AudioManager->SFXClass)
                    {
                        CritAC->SoundClassOverride = MyGameInstance->AudioManager->SFXClass;
                    }
                }
            }
        }
    }

    // --- Local player only: Camera Shake + Screen Flash + Hit Stop on self ---
    // Only when the attack actually connected (not a miss).
    if (!playerData.isOtherClient && !bIsMissed)
    {
        // Camera shake
        if (UIManager)
        {
            const float ShakeIntensity = bIsCritical ? 1.0f : 0.5f;
            UIManager->PlayCombatCameraShake(ShakeIntensity);
            UIManager->ShowDamageScreenFlash();
        }
    }

    // --- Hit Stop: freeze this actor briefly so the impact feels weighty ---
    // Not on miss вЂ” missing doesn't interrupt the actor's flow.
    if (!bIsMissed)
    {
        if (UWorld* W = GetWorld())
        {
            CustomTimeDilation = 0.0f;
            FTimerHandle HitStopTimer;
            TWeakObjectPtr<ABasicPlayer> WeakSelf(this);
            W->GetTimerManager().SetTimer(HitStopTimer, [WeakSelf]()
            {
                if (WeakSelf.IsValid())
                {
                    WeakSelf->CustomTimeDilation = 1.0f;
                }
            }, 0.06f, false);
        }
    }
}

void ABasicPlayer::ShowHealingEffect_Implementation(int32 Healing, const FString& SkillSlug)
{
    UE_LOG(LogTemp, Log, TEXT("Player %d healed for %d (skill: %s)"), GetActorId_Implementation(), Healing, *SkillSlug);

    // Look up skill definition for sound and VFX
    if (MyGameInstance)
    {
        if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

            // Heal sound from DataTable (overrides profile HealReceived if set)
            if (!Def.healSound.IsNull())
            {
                PlayEventSound(Def.healSound);
            }
            else
            {
                // Generic fallback sound from entity audio profile
                if (const FEntityAudioProfile* Profile = GetAudioProfile())
                {
                    PlayEventSound(Profile->HealReceived);
                }
            }

            // Heal Niagara VFX at HitSocket (green sparkles on target)
            if (!Def.healEffectNiagara.IsNull())
            {
                if (UNiagaraSystem* HealVFX = Def.healEffectNiagara.LoadSynchronous())
                {
                    FVector HealLoc = GetCombatPosition_Implementation();
                    FRotator HealRot = FRotator::ZeroRotator;
                    if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
                    {
                        HealLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
                        HealRot = GetMesh()->GetSocketRotation(Def.HitSocketName);
                    }
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HealVFX, HealLoc, HealRot);
                }
            }
        }
        else
        {
            if (const FEntityAudioProfile* Profile = GetAudioProfile())
            {
                PlayEventSound(Profile->HealReceived);
            }
        }
    }
    else
    {
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            PlayEventSound(Profile->HealReceived);
        }
    }

    // Floating green heal number
    if (UIManager)
    {
        if (UFloatingCombatTextManager* FCT = UIManager->GetFCTManager())
        {
            FCT->ShowDamage(GetCombatPosition_Implementation(), static_cast<float>(Healing), false, EDamageType::Heal);
        }

        // Screen flash only for local players
        if (!playerData.isOtherClient)
        {
            UIManager->ShowHealScreenFlash();
        }
    }
}

void ABasicPlayer::ShowManaRestoreEffect_Implementation(int32 ManaRestored)
{
    if (ManaRestored <= 0) return;

    UE_LOG(LogTemp, Log, TEXT("Player %d mana restored by %d"), GetActorId_Implementation(), ManaRestored);

    if (UIManager)
    {
        if (UFloatingCombatTextManager* FCT = UIManager->GetFCTManager())
        {
            FCT->ShowDamage(GetCombatPosition_Implementation(), static_cast<float>(ManaRestored), false, EDamageType::ManaRegen);
        }
    }
}

void ABasicPlayer::ShowBuffEffect_Implementation(const FAppliedEffectData& Effect)
{
    UE_LOG(LogTemp, Log, TEXT("Player %d received %s effect: %s (Value: %d, Duration: %.1f)"), 
        GetActorId_Implementation(), *Effect.effectType, *Effect.effectName, Effect.value, Effect.duration);

    if (!MyGameInstance) return;

    UDataTable* EffectTable = MyGameInstance->GetEffectDefinitionTable();
    if (!EffectTable) return;

    FName RowKey = FName(*Effect.effectName);
    const FEffectDefinitionRow* Row = EffectTable->FindRow<FEffectDefinitionRow>(RowKey, TEXT(""));
    if (!Row) return;

    // Play apply sound
    if (!Row->ApplySound.IsNull())
    {
        if (USoundBase* Snd = Row->ApplySound.LoadSynchronous())
        {
            UAudioComponent* AC = UGameplayStatics::SpawnSoundAtLocation(this, Snd, GetActorLocation());
            if (AC && MyGameInstance->AudioManager && MyGameInstance->AudioManager->SFXClass)
            {
                AC->SoundClassOverride = MyGameInstance->AudioManager->SFXClass;
            }
        }
    }

    // Spawn apply VFX
    if (!Row->ApplyVFX.IsNull())
    {
        if (UNiagaraSystem* VFX = Row->ApplyVFX.LoadSynchronous())
        {
            UNiagaraFunctionLibrary::SpawnSystemAttached(
                VFX, GetMesh(), NAME_None,
                FVector::ZeroVector, FRotator::ZeroRotator,
                EAttachLocation::SnapToTarget, true);
        }
    }
}

void ABasicPlayer::ShowDeathScreen()
{
    if (playerData.isOtherClient) return;

    if (UIManager)
    {
        UIManager->ShowDeathScreen(0);
    }
}

void ABasicPlayer::HideDeathScreen()
{
    if (UIManager)
    {
        UIManager->HideDeathScreen();
    }
}

void ABasicPlayer::OnRespawnClicked()
{
    // Only the local player can request a respawn, and only when dead
    if (playerData.isOtherClient || !playerData.characterData.bIsDead)
    {
        return;
    }

    if (!MyGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("BasicPlayer: OnRespawnClicked - MyGameInstance is null"));
        return;
    }

    UPlayerManager* PM = MyGameInstance->GetPlayerManager();
    if (!PM)
    {
        UE_LOG(LogTemp, Error, TEXT("BasicPlayer: OnRespawnClicked - PlayerManager is null"));
        return;
    }

    FClientDataStruct ClientData = MyGameInstance->GetCurrentClientData();
    PM->SendRespawnRequest(ClientData);
    UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Respawn requested for character %d"),
        playerData.characterData.characterId);
}

// Add these method implementations at the end of the file, before the closing brace

void ABasicPlayer::UpdatePlayerStats(const FPlayerStatsUpdateStruct& StatsUpdate)
{
	// Validate the stats update
	if (!PlayerAttributeParser::ValidateStatsData(StatsUpdate))
	{
		UE_LOG(LogTemp, Error, TEXT("BasicPlayer: Invalid stats update data received"));
		return;
	}

	// Check if this update is for this player
	if (StatsUpdate.characterId != playerData.characterData.characterId)
	{
		UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Stats update for different character (Expected: %d, Received: %d)"), 
			playerData.characterData.characterId, StatsUpdate.characterId);
		return;
	}

	// Update character data
	PlayerAttributeParser::UpdateCharacterDataFromStatsUpdate(playerData.characterData, StatsUpdate);

	UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Updated stats for character %d - Level: %d, HP: %d/%d, MP: %d/%d"),
		StatsUpdate.characterId, StatsUpdate.level,
		StatsUpdate.healthCurrent, StatsUpdate.healthMax,
		StatsUpdate.manaCurrent, StatsUpdate.manaMax);
}

void ABasicPlayer::ApplyServerMoveSpeed(float ServerMoveSpeed)
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		const float NewSpeed = ServerMoveSpeed * MoveSpeedScale;
		CMC->MaxWalkSpeed = NewSpeed;
		UE_LOG(LogTemp, Log, TEXT("BasicPlayer: MaxWalkSpeed set to %.1f (server move_speed=%.1f, scale=%.1f)"),
			NewSpeed, ServerMoveSpeed, MoveSpeedScale);
	}
}

void ABasicPlayer::ProcessStatsUpdate(const FPlayerStatsUpdateStruct& StatsUpdate)
{
	// Capture old level BEFORE UpdatePlayerStats overwrites characterData
	const int32 OldLevel = playerData.characterData.characterLevel;

	// Update HP/MP/level on the character data
	UpdatePlayerStats(StatsUpdate);

	// Refresh the HP/MP HUD
	RefreshHUD();

	// Signal the loading screen gate: HUD has real server data.
	// GameInstance guards against double-firing internally.
	if (!playerData.isOtherClient && MyGameInstance)
	{
		MyGameInstance->NotifyStatsReceived();
	}

	// --- Death detection (#1) ---
	// The server signals death via stats_update with health.current == 0.
	// Transition into dead state only if we are not already dead to avoid
	// re-triggering sounds / anim notifications on every repeated packet.
	if (StatsUpdate.healthCurrent == 0 && !playerData.characterData.bIsDead)
	{
		SetDead_Implementation(true);
	}

	// Sync experience fields only when the packet actually carries them.
	// Partial packets (e.g. regen ticks) omit the "experience" block so all
	// four fields arrive as 0 -- we must not overwrite the last known values.
	const bool bHasExpData = (StatsUpdate.experienceCurrent > 0
		|| StatsUpdate.experienceNextLevel  > 0
		|| StatsUpdate.experienceLevelStart > 0);
	if (bHasExpData)
	{
		playerData.characterData.characterExperiencePoints = StatsUpdate.experienceCurrent;
		playerData.characterData.characterExpForLevelStart = StatsUpdate.experienceLevelStart;
		playerData.characterData.characterExpForLevelEnd   = StatsUpdate.experienceNextLevel;
		playerData.characterData.characterExperienceDebt   = StatsUpdate.experienceDebt;
		UpdateExperienceData();
	}

	// Forward every stats_update to PlayerStatsManager so all subscribed widgets
	// (PlayerStatsWidget, PlayerExperienceWidget) receive the authoritative snapshot.
	if (MyGameInstance && !playerData.isOtherClient)
	{
		if (UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
		{
			StatsMgr->ApplyStatsUpdate(StatsUpdate);
		}
	}

	// Apply move_speed attribute from the server to the CharacterMovementComponent.
	// Server is authoritative: MaxWalkSpeed = move_speed * MoveSpeedScale, no client-side modifiers.
	if (!playerData.isOtherClient)
	{
		const FStatAttributeEntry* SpeedAttr = StatsUpdate.attributes.FindByPredicate(
			[](const FStatAttributeEntry& A){ return A.slug.Equals(TEXT("move_speed"), ESearchCase::IgnoreCase); });

		if (SpeedAttr && SpeedAttr->effective > 0.f)
		{
			ApplyServerMoveSpeed(SpeedAttr->effective);
		}
	}

	// Forward the full active-effects list to the HUD widget (replaces previous snapshot).
	// ActiveEffectsWidget keeps its own per-second tick for countdown labels.
	if (UIManager)
	{
		if (UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget())
		{
			if (UActiveEffectsWidget* AEW = PIW->GetActiveEffectsWidget())
			{
				AEW->RefreshEffects(StatsUpdate.activeEffects);
			}
		}
	}

	// Log each effect for diagnostics and fire ShowBuffEffect for Blueprint hooks.
	for (const FActiveEffectEntry& Effect : StatsUpdate.activeEffects)
	{
		FAppliedEffectData EffectData;
		EffectData.effectName = Effect.slug;
		EffectData.effectType = Effect.effectTypeSlug;
		if (Effect.expiresAt > 0)
		{
			const int64 NowSec = static_cast<int64>(FDateTime::UtcNow().ToUnixTimestamp());
			EffectData.duration = static_cast<float>(FMath::Max<int64>(Effect.expiresAt - NowSec, 0));
		}
		EffectData.value = static_cast<int32>(Effect.value);
		ShowBuffEffect_Implementation(EffectData);
	}

	UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Processed stats update and refreshed UI"));
}

void ABasicPlayer::HandleStatsManagerUpdate(const FPlayerStatsUpdateStruct& NewStats)
{
	// Log BEFORE any early return so we can see if the function is ever called at all.
	UE_LOG(LogTemp, Warning, TEXT("[EFFECTS] HandleStatsManagerUpdate: ENTER charId=%d myCharId=%d isOtherClient=%d effects=%d"),
		NewStats.characterId, playerData.characterData.characterId, (int)playerData.isOtherClient, NewStats.activeEffects.Num());

	// Only handle updates for the local player and for our own character.
	if (playerData.isOtherClient) return;
	if (NewStats.characterId != playerData.characterData.characterId) return;

	// Capture current vitals before the update so we can compute regen deltas.
	const int32 OldHP = playerData.characterData.characterCurrentHealth;
	const int32 OldMP = playerData.characterData.characterCurrentMana;

	// Use the authoritative parser so that max_health / max_mana attributes in
	// playerData are always kept in sync alongside the current vitals.
	// Previously only healthCurrent/manaCurrent were updated here, which meant
	// RefreshHUD could never find the max values and never drew the bars.
	PlayerAttributeParser::UpdateCharacterDataFromStatsUpdate(playerData.characterData, NewStats);

	RefreshHUD();

	// For passive regen ticks the server tags the update with source="regen".
	// Show floating combat text for the HP/MP gain so the player can see the numbers.
	if (NewStats.updateSource == TEXT("regen") && UIManager)
	{
		if (UFloatingCombatTextManager* FCT = UIManager->GetFCTManager())
		{
			const int32 HPGain = playerData.characterData.characterCurrentHealth - OldHP;
			const int32 MPGain = playerData.characterData.characterCurrentMana - OldMP;

			if (HPGain > 0)
				FCT->ShowDamage(GetCombatPosition_Implementation(), static_cast<float>(HPGain), false, EDamageType::Heal);
			if (MPGain > 0)
				FCT->ShowDamage(GetCombatPosition_Implementation(), static_cast<float>(MPGain), false, EDamageType::ManaRegen);
		}
	}

	// Forward activeEffects to the HUD buff bar. This mirrors ProcessStatsUpdate's
	// behaviour and ensures effects are visible even when stats arrive before the
	// player actor is registered (when ProcessStatsUpdate cannot be called).
	UE_LOG(LogTemp, Warning, TEXT("[EFFECTS] HandleStatsManagerUpdate: PASS UIManager=%s effects=%d"),
		UIManager ? TEXT("valid") : TEXT("NULL"), NewStats.activeEffects.Num());

	if (UIManager)
	{
		if (UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget())
		{
			UActiveEffectsWidget* AEW = PIW->GetActiveEffectsWidget();
			UE_LOG(LogTemp, Warning, TEXT("[EFFECTS] HandleStatsManagerUpdate: PIW=valid AEW=%s effects=%d"),
				AEW ? TEXT("valid") : TEXT("NULL"), NewStats.activeEffects.Num());
			if (AEW)
			{
				AEW->RefreshEffects(NewStats.activeEffects);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[EFFECTS] HandleStatsManagerUpdate: PIW is NULL"));
		}
	}
}


void ABasicPlayer::RefreshHUD()
{
	if (!PlayerHUD)
	{
		UE_LOG(LogTemp, Warning, TEXT("BasicPlayer: Cannot refresh HUD - PlayerHUD is null"));
		return;
	}

	// Primary source: characterAttributes populated from joinGameCharacter (stats.health.max)
	// and kept up-to-date by every stats_update via UpdateCharacterDataFromStatsUpdate.
	const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health"));
	const FAttributeDataStruct* ManaAttr   = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana"));

	float MaxHP  = (HealthAttr && HealthAttr->attributeValue > 0) ? static_cast<float>(HealthAttr->attributeValue) : 0.f;
	float MaxMP  = (ManaAttr   && ManaAttr->attributeValue   > 0) ? static_cast<float>(ManaAttr->attributeValue)   : 0.f;

	// Fallback: if characterAttributes are still empty (race condition between spawn
	// and first stats_update), read from the PlayerStatsManager cache.
	if (MaxHP <= 0.f && MyGameInstance)
	{
		if (const UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
		{
			const FPlayerStatsUpdateStruct& Cached = StatsMgr->GetCachedStats();
			if (Cached.characterId == playerData.characterData.characterId)
			{
				MaxHP = static_cast<float>(Cached.healthMax);
				MaxMP = static_cast<float>(Cached.manaMax);
			}
		}
	}

	if (MaxHP > 0.f)
	{
		PlayerHUD->SetHP(static_cast<float>(playerData.characterData.characterCurrentHealth), MaxHP);
	}

	if (MaxMP >= 0.f && (MaxHP > 0.f || MaxMP > 0.f))
	{
		PlayerHUD->SetMana(static_cast<float>(playerData.characterData.characterCurrentMana), MaxMP);
	}
}

void ABasicPlayer::PlayCombatSoundEvent(ECombatSoundSlot Slot)
{
    // Sounds are intentionally suppressed for remote (other-client) players.
    // However, the CastRelease projectile spawn must happen for ALL players so
    // that skills cast by other players are visually visible on this client (Bug 3 fix).
    // Non-CastRelease events are skipped entirely for remote players.
    if (playerData.isOtherClient && Slot != ECombatSoundSlot::CastRelease)
    {
        return;
    }

    switch (Slot)
    {
    case ECombatSoundSlot::VoiceAttack:
    {
        // Priority 1: per-entity per-skill override (DT_EntitySkillVoiceOverrides, key "warrior_m|basic_attack")
        if (MyGameInstance)
        {
            if (UEntityAudioRepository* Repo = MyGameInstance->GetEntityAudioRepository())
            {
                if (const FEntitySkillVoiceOverride* Override =
                        Repo->FindSkillVoiceOverride(AudioProfileId, FName(*ActiveAnimSkillSlug)))
                {
                    if (Override->VoiceAttack.Num() > 0)
                    {
                        PlayEventSound(Override->VoiceAttack[
                            FMath::RandRange(0, Override->VoiceAttack.Num() - 1)]);
                        break;
                    }
                }
            }
        }
        // Priority 2: generic entity melee voice pool from the audio profile
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            if (Profile->VoiceAttack.Num() > 0)
            {
                const int32 Idx = FMath::RandRange(0, Profile->VoiceAttack.Num() - 1);
                PlayEventSound(Profile->VoiceAttack[Idx]);
            }
        }
        break;
    }

    case ECombatSoundSlot::CastVoice:
    {
        // Cast-start voice: Priority 1 вЂ” skill-specific sound (same for any caster)
        //                   Priority 2 вЂ” this player's entity audio profile (VoiceCastStart pool)
        if (!MyGameInstance) break;
        if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(ActiveAnimSkillSlug);
            if (!Def.castStartVoice.IsNull())
            {
                PlayEventSound(Def.castStartVoice);
                break;
            }
        }
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            bool bVoicePlayed = false;

            // Priority 2: per-entity per-skill override (DT_EntitySkillVoiceOverrides, key "warrior_m|fireball")
            if (UEntityAudioRepository* Repo = MyGameInstance->GetEntityAudioRepository())
            {
                if (const FEntitySkillVoiceOverride* Override =
                        Repo->FindSkillVoiceOverride(AudioProfileId, FName(*ActiveAnimSkillSlug)))
                {
                    if (Override->CastStartVoice.Num() > 0)
                    {
                        PlayEventSound(Override->CastStartVoice[
                            FMath::RandRange(0, Override->CastStartVoice.Num() - 1)]);
                        bVoicePlayed = true;
                    }
                }
            }

            // Priority 3: generic entity cast-start voice pool
            if (!bVoicePlayed && Profile->VoiceCastStart.Num() > 0)
            {
                PlayEventSound(Profile->VoiceCastStart[
                    FMath::RandRange(0, Profile->VoiceCastStart.Num() - 1)]);
            }
        }
        break;
    }

    case ECombatSoundSlot::SwingSound:
    {
        if (!MyGameInstance) break;

        // Priority 1: equipped main-hand weapon's swing sound (sword woosh в‰  staff swish в‰  unarmed)
        if (UEquipmentManager* EquipMgr = MyGameInstance->GetEquipmentManager())
        {
            const FEquipmentSlotData& MainHand = EquipMgr->GetSlot(TEXT("main_hand"));
            if (MainHand.bIsOccupied && !MainHand.itemSlug.IsEmpty())
            {
                if (UItemManager* ItemMgr = MyGameInstance->GetItemManager())
                {
                    const FItemVisualData VisData = ItemMgr->GetItemVisualDataBySlug(MainHand.itemSlug);
                    if (!VisData.EquippedSwingSound.IsNull())
                    {
                        PlayEventSound(VisData.EquippedSwingSound);
                        break;
                    }
                }
            }
        }

        // Priority 2: skill-level swing sound (generic fallback)
        if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(ActiveAnimSkillSlug);
            if (!Def.swingSound.IsNull())
            {
                PlayEventSound(Def.swingSound);
            }
        }
        break;
    }

    case ECombatSoundSlot::CastRelease:
    {
        if (!MyGameInstance) break;
        USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository();
        if (!Repo) break;

        const FSkillDefinitionData& Def = Repo->GetDefinition(ActiveAnimSkillSlug);

        // Sounds and VFX are only played for the local player.
        if (!playerData.isOtherClient)
        {
        // Release sound (e.g. fireball launch, arrow release)
        if (!Def.castEndSound.IsNull())
        {
            PlayEventSound(Def.castEndSound);
        }

        // Release Niagara VFX at CastSocket (e.g. muzzle flash on hands, departing glow).
        // Skip when a projectile class is set: castEndEffectNiagara is used as the projectile
        // trail (attached TrailVFX component) вЂ” spawning it here as well would leave a static
        // copy frozen at the cast socket.
        if (!Def.castEndEffectNiagara.IsNull() && Def.projectileClass.IsNull())
        {
            if (UNiagaraSystem* VFX = Def.castEndEffectNiagara.LoadSynchronous())
            {
                FVector SpawnLoc = GetActorLocation();
                FRotator SpawnRot = GetActorRotation();
                if (Def.CastSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.CastSocketName))
                {
                    SpawnLoc = GetMesh()->GetSocketLocation(Def.CastSocketName);
                    SpawnRot = GetMesh()->GetSocketRotation(Def.CastSocketName);
                }
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX, SpawnLoc, SpawnRot);
            }
        }
        // Release voice: Priority 1 вЂ” skill-specific (castReleaseVoice), Priority 2 вЂ” player pool
        {
            TSoftObjectPtr<USoundBase> ReleaseVoice = Def.castReleaseVoice;
            if (!ReleaseVoice.IsNull())
            {
                PlayEventSound(ReleaseVoice);
            }
            else
            {
                // Fallback to entity audio profile:
                if (const FEntityAudioProfile* Profile = GetAudioProfile())
                {
                    bool bVoicePlayed = false;

                    // Priority 2: per-entity per-skill override (DT_EntitySkillVoiceOverrides)
                    if (UEntityAudioRepository* RepoAudio = MyGameInstance->GetEntityAudioRepository())
                    {
                        if (const FEntitySkillVoiceOverride* Override =
                            RepoAudio->FindSkillVoiceOverride(AudioProfileId, FName(*ActiveAnimSkillSlug)))
                        {
                            if (Override->CastReleaseVoice.Num() > 0)
                            {
                                PlayEventSound(Override->CastReleaseVoice[
                                    FMath::RandRange(0, Override->CastReleaseVoice.Num() - 1)]);
                                bVoicePlayed = true;
                            }
                        }
                    }

                    // Priority 3: generic entity release voice pool
                    if (!bVoicePlayed && Profile->VoiceCastRelease.Num() > 0)
                    {
                        PlayEventSound(Profile->VoiceCastRelease[
                            FMath::RandRange(0, Profile->VoiceCastRelease.Num() - 1)]);
                    }
                }
            }
        }
        } // end if (!playerData.isOtherClient) вЂ” sounds/VFX section

        // Spawn projectile if defined (preferred over frame-0 spawn вЂ” fires at correct cast-release timing)
        // This runs for both local and remote players so skills from other characters are visible (Bug 3 fix).
        if (!Def.projectileClass.IsNull())
        {
            UClass* ProjClass = Def.projectileClass.LoadSynchronous();
            if (ProjClass && GetWorld())
            {
                FName ProjSocket = (Def.CastSocketName != NAME_None) ? Def.CastSocketName : FName(TEXT("ProjectileSpawn"));
                FVector SpawnLoc = GetMesh() && GetMesh()->DoesSocketExist(ProjSocket)
                    ? GetMesh()->GetSocketLocation(ProjSocket)
                    : GetActorLocation() + FVector(0, 0, BaseEyeHeight);

                FRotator SpawnRot = GetActorRotation();
                AActor* TargetActor = nullptr;
                UCombatSystemManager* CombatMgr = MyGameInstance->GetCombatSystemManager();
                if (CurrentTargetId > 0 && CombatMgr)
                {
                    TScriptInterface<ICombatable> TargetCombatable =
                        CombatMgr->FindCombatableById(CurrentTargetId, CurrentTargetType);
                    if (TargetCombatable.GetObject() && IsValid(TargetCombatable.GetObject()))
                    {
                        TargetActor = Cast<AActor>(TargetCombatable.GetObject());
                        if (TargetActor)
                        {
                            SpawnRot = (TargetActor->GetActorLocation() - SpawnLoc).Rotation();
                        }
                    }
                }

                FActorSpawnParameters Params;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                Params.Instigator = this;
                AActor* Spawned = GetWorld()->SpawnActor<AActor>(ProjClass, SpawnLoc, SpawnRot, Params);

                // If it is a BaseMMOProjectile, configure it now
                if (ABaseMMOProjectile* Proj = Cast<ABaseMMOProjectile>(Spawned))
                {
                    // Use the pre-computed swing time so HideCastBar's CurrentCastTime reset
                    // (which fires at the same T=castTime) cannot corrupt the speed calculation.
                    float CalcSpeed = 0.0f;
                    const float SwingSeconds = CurrentSwingSeconds;
                    if (IsValid(TargetActor) && SwingSeconds > 0.001f)
                    {
                        const float Dist = FVector::Dist(SpawnLoc, TargetActor->GetActorLocation());
                        CalcSpeed = Dist / SwingSeconds;
                        UE_LOG(LogTemp, Log, TEXT("[PlayerAnim] CastRelease: dist=%.0f swing=%.3fs в†’ projectile speed=%.0f"),
                            Dist, SwingSeconds, CalcSpeed);
                    }
                    Proj->SetupProjectile(ActiveAnimSkillSlug, GetActorId_Implementation(), TargetActor, CalcSpeed);
                }

                UE_LOG(LogTemp, Log, TEXT("[PlayerAnim] CastRelease: spawned projectile '%s' for skill '%s'"),
                    *ProjClass->GetName(), *ActiveAnimSkillSlug);
            }
        }
        break;
    }

    default:
        break;
    }
}

void ABasicPlayer::ShowCastBar_Implementation(float CastTime, const FString& SkillName)
{
    // Store cast time for ALL players (including remote).
    // CurrentSwingSeconds = Duration - CastTime is pre-computed in PlaySkillAnimation
    // and drives the projectile speed calculation.  If we skip this for remote players
    // the swing time defaults to full Duration, making the projectile 2-3x too slow.
    CurrentCastTime = CastTime;

    if (playerData.isOtherClient) return;

    bIsCasting = true;

    // Stop any ongoing approach / auto-attack and freeze the character
    StopAutoAttack();
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        MoveComp->DisableMovement();
    }

    // Gameplay timer вЂ” unlocks movement after CastTime regardless of UI state.
    // This is the authoritative unlock path; HideCastBar_Implementation (called
    // by the server on interrupt/cancel) cancels this timer.
    if (CastTime > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            CastBarTimerHandle,
            this, &ABasicPlayer::HideCastBar_Implementation,
            CastTime, false);
    }

    if (UIManager)
    {
        if (UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget())
        {
            if (UCastBarWidget* CastBar = PIW->GetCastBarWidget())
            {
                CastBar->ShowCastBar(CastTime, SkillName);
            }
        }
    }
}

void ABasicPlayer::HideCastBar_Implementation()
{
    if (playerData.isOtherClient) return;

    CurrentCastTime = 0.0f;

    // Cancel the gameplay timer in case this was called early (interrupt/cancel from server).
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CastBarTimerHandle);
    }

    if (bIsCasting)
    {
        bIsCasting = false;
        if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
        {
            MoveComp->SetMovementMode(MOVE_Walking);
        }
    }

    if (UIManager)
    {
        if (UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget())
        {
            if (UCastBarWidget* CastBar = PIW->GetCastBarWidget())
            {
                CastBar->HideCastBar();
            }
        }
    }
}













void ABasicPlayer::PlayEmoteForCharacter(const FString& EmoteSlug, const FString& AnimationName)
{
    if (EmoteComponent)
    {
        EmoteComponent->PlayEmoteBySlug(EmoteSlug, AnimationName);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// WIO (World Interactive Objects) interaction
// ─────────────────────────────────────────────────────────────────────────────

void ABasicPlayer::HandleWIOActorSpawned(AWorldInteractiveObjectActor* SpawnedActor)
{
    if (SpawnedActor)
    {
        SpawnedActor->OnProximityChanged.AddDynamic(this, &ABasicPlayer::HandleWIOProximityChanged);
    }
}

void ABasicPlayer::HandleWIOProximityChanged(AWorldInteractiveObjectActor* WIOActor, bool bInRange)
{
    if (!WIOActor) return;

    if (bInRange)
    {
        TrackedWIOActor = WIOActor;

        // Show interaction prompt
        if (UIManager)
        {
            UIManager->ShowWIOInteractionPrompt(WIOActor->GetObjectData().ObjectId);
        }
    }
    else
    {
        // Only clear if it's the same actor we were tracking
        if (TrackedWIOActor == WIOActor)
        {
            TrackedWIOActor = nullptr;
            if (UIManager)
            {
                UIManager->HideWIOInteractionPrompt();
            }

            // Cancel channel if we walk out of range while channeling
            CancelWIOChannelIfActive();
        }
    }
}

void ABasicPlayer::TryInteractWithWIO()
{
    if (!TrackedWIOActor || !MyGameInstance)
    {
        return;
    }

    UWorldObjectManager* WOM = MyGameInstance->GetWorldObjectManager();
    if (!WOM)
    {
        return;
    }

    const int32 ObjectId = TrackedWIOActor->GetObjectData().ObjectId;

    // If already channeling on this object, cancel instead
    if (WOM->IsChanneling() && WOM->GetActiveChannelObjectId() == ObjectId)
    {
        WOM->RequestCancelChannel(ObjectId);
        if (UIManager)
        {
            UIManager->HideWIOChannelBar();
        }
        return;
    }

    WOM->RequestInteract(ObjectId);
}

void ABasicPlayer::CancelWIOChannelIfActive()
{
    if (!MyGameInstance) return;

    UWorldObjectManager* WOM = MyGameInstance->GetWorldObjectManager();
    if (WOM && WOM->IsChanneling())
    {
        WOM->CancelActiveChannel();
        if (UIManager)
        {
            UIManager->HideWIOChannelBar();
        }
    }
}
