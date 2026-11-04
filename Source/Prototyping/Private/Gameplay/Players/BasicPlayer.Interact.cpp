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
#include "Gameplay/Mobs/MOBManager.h"
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
#include "Utils/NetCore/TargetingRule.h"
#include "Utils/NetCore/InteractionModeRule.h"
#include "DrawDebugHelpers.h"
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

    // Zoom-independent capture: gameplay range from the player, ray travels
    // camera->player plus that range. Primary is a sphere sweep (beam thickness).
    const float GameplayRange = CursorInteractionComponent ? CursorInteractionComponent->GetCrosshairTraceRange() : 1500.0f;
    const float ConeDot = CursorInteractionComponent ? CursorInteractionComponent->GetSoftTargetConeDot() : 0.985f;
    const float TraceRadius = CursorInteractionComponent ? CursorInteractionComponent->GetCrosshairTraceRadius() : 30.0f;
    const float CameraToPlayer = FVector::Dist(CameraLocation, GetActorLocation());
    const float TraceLen = FTargetingRule::EffectiveTraceLength(CameraToPlayer, GameplayRange);
    FVector TraceEnd = CameraLocation + CameraForward * TraceLen;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    FHitResult Hit;
    const bool bHit = TraceRadius > 0.0f
        ? GetWorld()->SweepSingleByChannel(Hit, CameraLocation, TraceEnd, FQuat::Identity,
            ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius), Params)
        : GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Pawn, Params);
    ABasicNPC* HitNPC = bHit ? Cast<ABasicNPC>(Hit.GetActor()) : nullptr;

    // Fallback: find the NPC closest to the camera crosshair within range
    // (registry iteration, O(registered NPCs) — no world scan).
    if (!HitNPC)
    {
        if (UNPCManager* NpcMgr = MyGameInstance ? MyGameInstance->GetNPCManager() : nullptr)
        {
            ABasicNPC* BestNPC = nullptr;
            float BestDot = ConeDot; // crosshair cone from DA_WorldInteractionConfig

            for (ABasicNPC* NPC : NpcMgr->GetInteractableNPCs())
            {
                if (!IsValid(NPC)) continue;

                const float Dist = FVector::Dist(GetActorLocation(), NPC->GetActorLocation());
                if (!FTargetingRule::IsInRange(Dist, GameplayRange)) continue;

                const FVector ToNPC = (NPC->GetActorLocation() - CameraLocation).GetSafeNormal();
                const float Dot = FVector::DotProduct(CameraForward, ToNPC);
                if (FTargetingRule::IsInCone(Dot, BestDot))
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
    }

    if (HitNPC && !HitNPC->IsNPCInteractable())
    {
        UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: CheckForNPC hit NPC '%s' but IsNPCInteractable() is false"), *HitNPC->GetNPCName());
    }

    if (CursorInteractionComponent && CursorInteractionComponent->GetDrawTargetTraceDebug())
    {
        ABasicNPC* DbgNPC = (HitNPC && IsValid(HitNPC)) ? HitNPC : nullptr;
        DrawDebugLine(GetWorld(), CameraLocation, DbgNPC ? DbgNPC->GetActorLocation() : TraceEnd,
            DbgNPC ? FColor::Green : FColor::Red, false, 0.0f, 0, 2.0f);
    }

    // Talk range == click range (GetInteractionRange). The crosshair trace
    // reaches 1500 units, but F must not open dialogue the click handler
    // would refuse — and the hint must not promise it either.
    if (HitNPC && FVector::Dist(GetActorLocation(), HitNPC->GetActorLocation()) > GetInteractionRange())
    {
        HitNPC = nullptr;
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
        UE_LOG(LogGameplay, Verbose, TEXT("[NPC] Player moved %.0f units from NPC %d (threshold %.0f). Force-closing NPC windows."),
            Dist2D, ActiveNpcId, CloseThreshold);
        UDialogueManager* DlgMgr = MyGameInstance->GetDialogueManager();
        UIManager->ForceCloseAllNPCWindows(DlgMgr);
    }
}

void ABasicPlayer::OnInteractInput()
{
	UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: OnInteractInput triggered"));

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

	// Priority 4: NPC dialogue (range-gated like the click path: no
	// cross-map talks — the hint is only shown inside this radius too).
	if (TrackedNPC && FVector::Dist(GetActorLocation(), TrackedNPC->GetActorLocation()) <= GetInteractionRange())
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

    // Gameplay range is measured FROM THE PLAYER so zoom never shrinks capture;
    // the ray itself travels camera->player plus that range (zoom-compensated).
    const float GameplayRange = CursorInteractionComponent ? CursorInteractionComponent->GetCrosshairTraceRange() : 1500.0f;
    const float ConeDot = CursorInteractionComponent ? CursorInteractionComponent->GetSoftTargetConeDot() : 0.985f;
    const float TraceRadius = CursorInteractionComponent ? CursorInteractionComponent->GetCrosshairTraceRadius() : 30.0f;
    const float CameraToPlayer = FVector::Dist(CameraLocation, GetActorLocation());
    const float TraceLen = FTargetingRule::EffectiveTraceLength(CameraToPlayer, GameplayRange);
    const FVector TraceEnd = CameraLocation + CameraForward * TraceLen;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    ABasicMOB* SoftTarget = nullptr;

    // Primary: sphere sweep against pawn capsules (beam thickness from DA).
    // A thin ray misses small mobs at range; the sphere fixes that without
    // the wide-cone stickiness. Radius 0 falls back to a plain line trace.
    FHitResult Hit;
    const bool bPrimaryHit = TraceRadius > 0.0f
        ? GetWorld()->SweepSingleByChannel(Hit, CameraLocation, TraceEnd, FQuat::Identity,
            ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius), Params)
        : GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Pawn, Params);
    if (bPrimaryHit)
    {
        ABasicMOB* Mob = Cast<ABasicMOB>(Hit.GetActor());
        if (Mob && !Mob->GetMOBIsDead())
        {
            SoftTarget = Mob;
        }
    }

    // Fallback: dot-product cone assist (handles meshes that block ECC_Pawn poorly).
    // Registry iteration, O(registered mobs) — no world scan. Distance and cone
    // from DA; distance measured from the player (zoom-independent).
    if (!SoftTarget)
    {
        if (UMOBManager* MobMgr = MyGameInstance ? MyGameInstance->GetMOBManager() : nullptr)
        {
            const FVector PlayerLoc = GetActorLocation();
            float BestDot = ConeDot; // crosshair cone from DA_WorldInteractionConfig
            MobMgr->ForEachMob([&](ABasicMOB* Mob)
            {
                if (!Mob || Mob->GetMOBIsDead()) return;

                const float Dist = FVector::Dist(PlayerLoc, Mob->GetActorLocation());
                if (!FTargetingRule::IsInRange(Dist, GameplayRange)) return;

                const FVector ToMob = (Mob->GetActorLocation() - CameraLocation).GetSafeNormal();
                const float Dot = FVector::DotProduct(CameraForward, ToMob);
                if (FTargetingRule::IsInCone(Dot, BestDot))
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
            });
        }
    }

    // Debug: crosshair<->trace alignment (DA flag, off by default — zero cost when off).
    if (CursorInteractionComponent && CursorInteractionComponent->GetDrawTargetTraceDebug())
    {
        ABasicMOB* DbgMob = (SoftTarget && IsValid(SoftTarget)) ? SoftTarget : nullptr;
        DrawDebugLine(GetWorld(), CameraLocation, DbgMob ? DbgMob->GetActorLocation() : TraceEnd,
            DbgMob ? FColor::Green : FColor::Red, false, 0.0f, 0, 2.0f);
        if (DbgMob)
            DrawDebugSphere(GetWorld(), DbgMob->GetActorLocation(), 30.0f, 12, FColor::Green, false, 0.0f, 0, 2.0f);
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

            // Update player target frame HP (dead shows 0, like the mob frame).
            const int32 PlayerHP = LockedPlayerTarget->GetIsDead()
                ? 0 : LockedPlayerTarget->playerData.characterData.characterCurrentHealth;
            UIManager->UpdateMobTargetFrameHP(PlayerHP, LockedPlayerTarget->GetMaxHealth_Implementation());
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

    // Center-screen reticle (BDO rule: cursor xor crosshair). The mouse cursor is
    // the single source of truth: cursor visible = UI/cursor mode, reticle stays
    // hidden; cursor hidden (RMB drag / no UI / no Alt) = combat mode, reticle shows
    // the camera-crosshair trace state. Lock wins over soft-target.
    if (UIManager)
    {
        const bool bCursorVisible = PC && PC->bShowMouseCursor;
        const bool bDead = playerData.characterData.bIsDead;
        ECrosshairState Reticle = ECrosshairState::Neutral;
        if (LockedTarget && IsValid(LockedTarget))
            Reticle = LockedTarget->GetMOBIsDead() ? ECrosshairState::Dead : ECrosshairState::Locked;
        else if (SoftTarget && IsValid(SoftTarget))
            Reticle = SoftTarget->GetMOBIsDead() ? ECrosshairState::Dead : ECrosshairState::HoverMob;
        if (!FInteractionModeRule::ShouldShowCrosshair(bCursorVisible, bDead))
            Reticle = ECrosshairState::None;
        UIManager->SetTargetReticle(Reticle);
    }

    // Cull world head plates beyond the configured distance (the hard lock +
    // target frame survive — only the in-world plate hides, like AAA MMOs).
    const float CullDist = CursorInteractionComponent ? CursorInteractionComponent->GetMobHeadCullDistance() : 2500.f;
    auto ShouldShowPlate = [&](ABasicMOB* Mob) -> bool
    {
        if (!Mob || !IsValid(Mob)) return false;
        const float D = FVector::Dist(GetActorLocation(), Mob->GetActorLocation());
        return !FTargetingRule::ShouldCullHeadPlate(D, CullDist);
    };

    // Show/hide head widget using a cached pointer пїЅ no TActorIterator every Tick
    if (PrevSoftTarget != EffectiveTarget)
    {
        // Hide the widget on the mob that just left the crosshair
        // (skip if it is the hard-locked target пїЅ SetLockedTarget already manages it)
        if (PrevSoftTarget && PrevSoftTarget != LockedTarget && IsValid(PrevSoftTarget))
        {
            PrevSoftTarget->MobHeadInfo->ShowWidget(false);
        }

        // Show the widget on the new effective target (culled beyond range)
        if (EffectiveTarget && IsValid(EffectiveTarget))
        {
            EffectiveTarget->MobHeadInfo->ShowWidget(ShouldShowPlate(EffectiveTarget));
        }

        if (CursorInteractionComponent && CursorInteractionComponent->GetDrawTargetTraceDebug())
        {
            const float D = EffectiveTarget && IsValid(EffectiveTarget)
                ? FVector::Dist(CameraLocation, EffectiveTarget->GetActorLocation()) : -1.0f;
            UE_LOG(LogGameplay, Warning, TEXT("[TargetTrace] soft-target -> %s (dist %.0f)"),
                EffectiveTarget && IsValid(EffectiveTarget) ? *EffectiveTarget->GetMOBUId() : TEXT("<none>"), D);
        }

        PrevSoftTarget = EffectiveTarget;
    }
}


