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

// Called every frame
void ABasicPlayer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    CRASH_GUARD("BasicPlayer::Tick");

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
        // Continuously face the locked target during casting so we track a moving mob
        if (bIsCasting && LockedTarget && !LockedTarget->GetMOBIsDead())
        {
            FaceLockedTarget();
        }
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

        ProximityHintAccumulator += DeltaTime;
        if (ProximityHintAccumulator >= 0.25f)
        {
            ProximityHintAccumulator = 0.0f;
            UpdateProximityHint();
        }
    }

    // Update player movement for remote player
    if (playerData.isOtherClient && !bPreviewMovementActive)
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
    if (IsValid(LockedTarget) && UIManager && !playerData.isOtherClient)
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
    if (playerData.characterData.bIsDead) return;
    if (bIsCasting) return;
    if (bIsPickingUp) return;

    // Block movement input while playing an emote.  NotifyMovementStarted will
    // interrupt the emote on the first input frame, then UnlockMovement fires
    // via HandleEmoteEnded on the next frame.
    if (EmoteComponent && EmoteComponent->IsPlayingEmote())
    {
        EmoteComponent->NotifyMovementStarted();
        return;
    }

    if (bMovementLocked) return;

    // Block movement while an attack animation is playing.  The current attack
    // animation must complete before the player can move or start a new action.
    // Future auto-attack timer is cancelled here so the attack loop does not
    // restart after this animation finishes and re-lock movement.
    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        if (AnimInst->bIsAttacking)
        {
            if (bIsAutoAttacking) StopAutoAttack();
            bHasDesiredMeshYaw = false;
            return;
        }
    }

    // Cancel WIO channel on any movement input
    CancelWIOChannelIfActive();

    // Manual WASD input interrupts the combat approach (auto-attack) but
    // allows non-combat approaches (harvest, NPC, pickup) to continue
    // so the player walks to the corpse/item without being cancelled by held keys.
    if (bIsAutoAttacking || bIsApproachingTarget)
    {
        const bool bIsNonCombatApproach = (PendingInteraction == EPendingInteraction::Harvest ||
                                           PendingInteraction == EPendingInteraction::TalkNPC   ||
                                           PendingInteraction == EPendingInteraction::PickupItem);
        if (!bIsNonCombatApproach)
        {
            UE_LOG(LogTemp, Log, TEXT("BasicPlayer: Auto-attack interrupted by manual movement"));
            StopAutoAttack();
            // Clear combat-facing rotation so the character doesn't run sideways
            // toward the former locked target when the player moves manually.
            bHasDesiredMeshYaw = false;
        }
    }

    const FVector2D MoveValue = Value.Get<FVector2D>(); // NOT normalized пїЅ keep X/Y separate
    const float CameraYaw = Controller->GetControlRotation().Yaw;

    // Use camera-relative movement when cursor is hidden (free-look mode),
    // even without RMB held.  This prevents stutter from mesh rotation lag
    // when press/release RMB toggles between mesh- and camera-relative modes.
    APlayerController* MovePC = Cast<APlayerController>(Controller);
    const bool bFreeLookMove = bIsRightMouseDown
        || (MovePC && !MovePC->bShowMouseCursor
            && !(UIManager && UIManager->HasUIWindowOpen()));

    if (bFreeLookMove)
    {
        // Camera-relative: W/S forward/back, A/D strafe
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
        // Strafing mode (no RMB, cursor visible): W/S move by mesh facing, A/D strafe left/right
        if (!FMath::IsNearlyZero(MoveValue.Y))
        {
            const FVector MeshForward = GetActorForwardVector();
            AddMovementInput(MeshForward, MoveValue.Y > 0.f ? 1.0f : -1.0f);
        }

        if (!FMath::IsNearlyZero(MoveValue.X))
        {
            const FVector MeshRight = GetActorRightVector();
            AddMovementInput(MeshRight, MoveValue.X);
        }
    }
}

void ABasicPlayer::Look(const FInputActionValue& Value)
{
    // Determine whether we are in free-look mode (cursor hidden, no UI window).
    // Free-look rotates the camera AND the character, exactly like RMB-hold.
    APlayerController* PC = Cast<APlayerController>(GetController());
    const bool bFreeLook = (!bIsRightMouseDown && !bIsLeftMouseDown)
        && PC && !PC->bShowMouseCursor
        && !(UIManager && UIManager->HasUIWindowOpen());

    // Only process mouse look when at least one mouse button is held,
    // UNLESS the cursor is hidden and no UI window is open (free-look mode).
    if (!bIsRightMouseDown && !bIsLeftMouseDown && !bFreeLook) return;

    // Dead players: allow camera look-around but keep the corpse mesh stationary.
    if (playerData.characterData.bIsDead)
    {
        if (Controller != nullptr)
        {
            const FVector2D LookValue = Value.Get<FVector2D>();
            if (!FMath::IsNearlyZero(LookValue.X))
            {
                AddControllerYawInput(LookValue.X * CameraYawSensitivity);
            }
            if (!FMath::IsNearlyZero(LookValue.Y))
            {
                AddControllerPitchInput(LookValue.Y * -CameraPitchSensitivity);
            }
        }
        return;
    }

    // Do not rotate the camera while the cursor is over a Visible UI element
    if (IsUIBlockingInteraction()) return;

    // If chat input is focused and player rotates camera, release keyboard
    // focus back to the game so WASD movement works without clicking away.
    if (UIManager && UIManager->GetChatWidget())
    {
        UIManager->GetChatWidget()->ReleaseInputFocus();
    }

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

            // RMB held (or free-look): character follows camera yaw — but not mid-cast.
            // The camera itself still rotates freely; only the mesh stays locked
            // toward the target until the animation finishes.
            if (bIsRightMouseDown || bFreeLook)
            {
                const bool bAnimLocked = (MyGameInstance && MyGameInstance->GetPlayerSkillManager()
                    && MyGameInstance->GetPlayerSkillManager()->IsSkillAnimationPlaying())
                    || bMovementLocked;  // lock rotation during harvest/pickup/emotes
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

