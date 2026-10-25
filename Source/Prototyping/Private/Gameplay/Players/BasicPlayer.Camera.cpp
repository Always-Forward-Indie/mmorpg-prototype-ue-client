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

void ABasicPlayer::OnRightMousePressed()
{
    if (playerData.characterData.bIsDead) return;

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
    if (!bLMBDragActive && CursorInteractionComponent && !IsUIBlockingInteraction() && !bIsCasting)
    {
        CursorInteractionComponent->HandleConfirmedClick();
    }
    else
    {
        UE_LOG(LogGameplay, Verbose, TEXT("CursorInteraction: LMB click BLOCKED вЂ” drag=%d, comp=%s, UIBlocking=%d"),
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
    if (UIManager && UIManager->HasUIWindowOpen()) return;

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        const bool bWasCursorVisible = PC->bShowMouseCursor;
        PC->bShowMouseCursor = false;

        if (bWasCursorVisible && !(UIManager && UIManager->GetAltCursorActive()))
        {
            PC->SetInputMode(FInputModeGameOnly());
        }
    }
}

void ABasicPlayer::RestoreCursorToUIManager()
{
    APlayerController* PC = Cast<APlayerController>(GetController());

    if (UIManager && PC)
    {
        // When Alt cursor mode is active and no UI windows are open, the input
        // mode is already GameAndUI (set when Alt was toggled).  Just show the
        // cursor without calling UpdateCursorAndInputMode() — a redundant
        // SetInputMode() triggers Slate focus re-evaluation that drops 1+ frames
        // of input, causing movement stutter during RMB cycles.
        if (!UIManager->HasUIWindowOpen() && UIManager->GetAltCursorActive())
        {
            PC->bShowMouseCursor = true;
            return;
        }

        // Cursor already hidden and should stay hidden (no UI, no Alt active).
        // Skip UpdateCursorAndInputMode entirely to avoid ClearKeyboardFocus +
        // SetAllUserFocusToGameViewport spam on every RMB release, which was
        // causing movement stutter during rapid RMB cycles.
        if (!PC->bShowMouseCursor && !UIManager->ShouldShowCursor())
        {
            return;
        }

        UIManager->UpdateCursorAndInputMode();
        return;
    }

    // Fallback if UIManager is not available
    if (PC)
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI());
    }
}

void ABasicPlayer::UpdateMeshRotation(float DeltaTime)
{
    if (playerData.characterData.bIsDead) return;
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
    if (playerData.characterData.bIsDead || bIsPickingUp || bIsCasting || bMovementLocked) return;
    if (EmoteComponent && EmoteComponent->IsPlayingEmote()) return;
    if (UIManager && UIManager->HasUIWindowOpen()) return;
    if (!Controller) return;

    // Blend out of any lingering skill animation (same as Move())
    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        if (AnimInst->bIsAttacking)
        {
            AnimInst->StopCurrentAttack(0.15f);
        }
    }

    if (bIsAutoAttacking || bIsApproachingTarget)
    {
        const bool bIsNonCombatApproach = (PendingInteraction == EPendingInteraction::Harvest ||
                                           PendingInteraction == EPendingInteraction::TalkNPC   ||
                                           PendingInteraction == EPendingInteraction::PickupItem);
        if (!bIsNonCombatApproach)
        {
            StopAutoAttack();
        }
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

