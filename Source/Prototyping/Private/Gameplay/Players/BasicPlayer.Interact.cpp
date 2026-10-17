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
	if (playerData.isOtherClient) return;
	if (!MyGameInstance) return;

	// Priority 1: Harvestable corpse
	if (UHarvestManager* HMgr = MyGameInstance->GetHarvestManager())
	{
		if (!HMgr->IsHarvesting())
		{
			if (ABasicMOB* Corpse = HMgr->GetNearestHarvestableCorpse(HMgr->GetMaxHarvestDistance()))
			{
				SetLockedTarget(Corpse);  // face the corpse before harvesting
				HMgr->TryHarvestSpecificCorpse(Corpse);
				return;
			}
		}
	}

	// Priority 2: Dropped item
	if (InventoryManager)
	{
		const float PickupRange = CursorInteractionComponent ? CursorInteractionComponent->GetItemPickupRange() : 180.f;
		if (ADroppedItemActor* Item = InventoryManager->GetNearestDroppedItem(PickupRange))
		{
			InventoryManager->PickupSpecificItem(Item);
			return;
		}
	}

	// Priority 3: Already harvested corpse (inspect loot)
	if (UHarvestManager* HMgr = MyGameInstance->GetHarvestManager())
	{
		if (ABasicMOB* Corpse = HMgr->GetNearestHarvestedCorpse(HMgr->GetMaxHarvestDistance()))
		{
			const int32 Uid = FCString::Atoi(*Corpse->GetMOBUId());
			HMgr->InspectCorpseLoot(Uid);
			return;
		}
	}

	// Priority 4: NPC dialogue
	if (TrackedNPC)
	{
		UDialogueManager* DlgManager = MyGameInstance->GetDialogueManager();
		if (DlgManager)
		{
			if (DlgManager->IsDialogueActive())
			{
				if (UIManager) UIManager->ForceCloseAllNPCWindows(DlgManager);
				else DlgManager->CloseDialogue();
				return;
			}

			if (UIManager)
			{
				const int32 ActiveNpcId = UIManager->GetActiveInteractionNpcId();
				if (ActiveNpcId > 0 && ActiveNpcId != TrackedNPC->GetNPCId())
				{
					UIManager->ForceCloseAllNPCWindows(DlgManager);
				}
			}

			DlgManager->OpenDialogue(TrackedNPC->GetNPCId());
			return;
		}
	}

	// Priority 5: World Interactive Object
	TryInteractWithWIO();
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

    // Stop attacking dead mob but keep the lock for harvesting.
    // Do NOT clear the target — other paths (DoAutoAttack, UpdateApproach,
    // TryCastSkillWithApproach) also preserve it for this purpose.
    if (IsValid(LockedTarget) && LockedTarget->GetMOBIsDead())
    {
        const bool bNonCombatApproaching =
            (PendingInteraction == EPendingInteraction::Harvest ||
             PendingInteraction == EPendingInteraction::TalkNPC ||
             PendingInteraction == EPendingInteraction::PickupItem) &&
            bIsApproachingTarget;

        if (bNonCombatApproaching)
        {
            bIsAutoAttacking = false;
            PendingSkillSlug.Empty();
            GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);
        }
        else
        {
            StopAutoAttack();
        }

        if (CursorInteractionComponent)
            CursorInteractionComponent->SetVisualLock(LockedTarget, EInteractableType::MOB_Harvestable);
    }

    // No auto-clear for RETURNING state, leash distance, or any other reason.
    // The player controls when to release the target (Tab, click on empty ground, etc.).

    // Effective target: hard lock takes priority over soft target
    ABasicMOB* EffectiveTarget = LockedTarget ? LockedTarget : SoftTarget;

    // Update skill system target — player target takes priority over mob soft target
    if (UIManager)
    {
        if (bIsSelfTargeted)
        {
            UIManager->SetSkillTarget(playerData.characterData.characterId, ECasterType::Self);
            UIManager->UpdateMobTargetFrameHP(
                playerData.characterData.characterCurrentHealth,
                GetMaxHealth_Implementation());
        }
        else if (LockedPlayerTarget)
        {
            const int32 PlayerId = LockedPlayerTarget->playerData.characterData.characterId;
            UIManager->SetSkillTarget(PlayerId, ECasterType::Player);

            // Update player target frame HP
            UIManager->UpdateMobTargetFrameHP(
                LockedPlayerTarget->playerData.characterData.characterCurrentHealth,
                LockedPlayerTarget->GetMaxHealth_Implementation());
        }
        else
        {
            const int32 EffectiveTargetId = EffectiveTarget
                ? FCString::Atoi(*EffectiveTarget->GetMOBUId()) : 0;
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


