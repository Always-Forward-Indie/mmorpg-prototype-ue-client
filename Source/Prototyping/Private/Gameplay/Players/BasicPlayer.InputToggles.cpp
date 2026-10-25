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

    LockMovement();
    UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: Movement locked for pickup"));

    // Safety timeout: unlock movement after 5s even if the pickup chain fails
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(PickupSafetyTimerHandle, this,
            &ABasicPlayer::UnlockMovementAfterPickup, 5.0f, false);
    }
}

void ABasicPlayer::UnlockMovementAfterPickup()
{
    if (!bIsPickingUp) return;
    bIsPickingUp = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PickupSafetyTimerHandle);
    }

    UnlockMovement();
    UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: Movement unlocked after pickup"));
}

void ABasicPlayer::LockMovement()
{
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        SavedMovementMode = MoveComp->MovementMode;

        // Drain pending movement input so no residual acceleration carries over
        ConsumeMovementInputVector();

        // Zero horizontal velocity to stop any slide/drift immediately,
        // but preserve Z so gravity-affected falling continues uninterrupted.
        FVector Vel = MoveComp->Velocity;
        Vel.X = 0.0f;
        Vel.Y = 0.0f;
        MoveComp->Velocity = Vel;
        MoveComp->UpdateComponentVelocity();
    }
    bMovementLocked = true;
}

void ABasicPlayer::UnlockMovement()
{
    bMovementLocked = false;
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        if (MoveComp->MovementMode != SavedMovementMode)
        {
            MoveComp->SetMovementMode(SavedMovementMode);
        }
    }
}

void ABasicPlayer::OnInventoryToggle()
{
    UE_LOG(LogGameplay, Warning, TEXT("Inventory toggle pressed"));
    
    if (InventoryManager)
    {
        InventoryManager->ToggleInventoryUI();
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("Inventory manager not found"));
    }
}

void ABasicPlayer::OnSkillsPanelToggle()
{
    UE_LOG(LogGameplay, Warning, TEXT("Skills panel toggle pressed"));
    
    if (UIManager)
    {
        UIManager->ToggleSkillsPanel();
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("UI manager not found"));
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
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
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
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
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
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
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
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
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
    const bool bNeedsApproach = !SkillSlug.IsEmpty() && GetLockedTargetActor()
        && SkillMgr && SkillMgr->GetEffectiveTargetPolicy(SkillSlug) != ESkillTargetPolicy::SelfOnly;
    if (bNeedsApproach)
    {
        TryCastSkillWithApproach(SkillSlug);
    }
    else
    {
        UIManager->GetSkillBarWidget()->CastSkillFromSlot(4);
    }
}

