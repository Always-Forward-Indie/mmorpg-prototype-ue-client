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
#include "Utils/NetCore/RemoteLandingRule.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "Data/CharacterVisualData.h"
#include "Engine/AssetManager.h"

void ABasicPlayer::SyncPawnTransformToPlayerData()
{
	const FVector PawnLoc = GetActorLocation();
	const FRotator PawnRot = GetActorRotation();
	playerData.characterData.characterPosition.positionX = PawnLoc.X;
	playerData.characterData.characterPosition.positionY = PawnLoc.Y;
	playerData.characterData.characterPosition.positionZ = PawnLoc.Z;
	playerData.characterData.characterPosition.rotationZ = PawnRot.Yaw;
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		playerData.characterData.bIsFalling = CMC->IsFalling();
	}
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
		playerData.characterData.bIsFalling = GetCharacterMovement()->IsFalling();

		// Send player movement to the game server (skipped in DevMode offline:
		// movement is local-only, no chunk to report to — avoids per-tick
		// Error spam from the send path).
		if (MyGameInstance && MyGameInstance->PlayerManager && !MyGameInstance->DevModeConfig.bEnabled)
		{
			MyGameInstance->PlayerManager->SendMovePlayerRequest(playerData);
		}
		else
		{
			UE_LOG(LogConnection, Verbose, TEXT("UpdateCurrentPlayerMovement: skipping move send (DevMode offline or tearing down)"));
		}

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
    // 1. Position: render-delayed snapshot interpolation (S10) with
    //    dead-reckoning fallback.
    //    While the snapshot buffer is healthy the actor is placed exactly on
    //    the sampled trajectory — no open-loop extrapolation past the latest
    //    packet, hence no jitter overshoot and no GapCorrection oscillation.
    //    While the buffer is short (stream start) or stale (long stall),
    //    open-loop dead reckoning keeps the actor moving; the next packets
    //    refill the buffer and interpolation resumes seamlessly.
    // -----------------------------------------------------------------------
    const FVector EffectiveVel = bRemoteIsMoving ? RemoteVelocity
        : FMath::VInterpTo(RemoteVelocity, FVector::ZeroVector, DeltaTime, 3.0f);
    if (!bRemoteIsMoving)
    {
        RemoteVelocity = EffectiveVel;
    }

    const FVector CurrentPos   = GetActorLocation();
    FVector SampledTarget;
    const bool bHaveSample = SampleMoveTarget(SampledTarget);
    FVector NewPosition;
    if (bHaveSample)
    {
        NewPosition = SampledTarget;
    }
    else
    {
        NewPosition = CurrentPos + EffectiveVel * DeltaTime;
    }
    // XY is placement; Z is interpolated toward the server height
    // so the actor steps onto rocks/stairs naturally.
    const FVector HeightRef = bHaveSample ? SampledTarget : TargetReceivedPosition;
    NewPosition.Z = FMath::FInterpTo(CurrentPos.Z, HeightRef.Z, DeltaTime, 12.0f);

    const float DistToTarget = FVector::Dist(CurrentPos, TargetReceivedPosition);

    if (DistToTarget > 1000.f)
    {
        // Hard-snap on large gaps (teleport / zone change).
        UE_LOG(LogGameplay, Verbose, TEXT("[FLIGHT] hard-snap %.0f (teleport/zone)"), DistToTarget);
        SetActorLocation(TargetReceivedPosition, false, nullptr, ETeleportType::TeleportPhysics);
        // Buffered past positions no longer describe this location.
        MoveSnapshotBuf.Reset();
    }
    else
    {
        SetActorLocation(NewPosition);
    }

    // -----------------------------------------------------------------------
    // 2. Rotation interpolation
    //    Linear lerp over the server update interval so angular velocity is
    //    synchronized with movement.  FindDeltaAngleDegrees handles
    //    the ±180° wrap-around correctly.
    // -----------------------------------------------------------------------
    {
        const float RotWindow  = FMath::Max(ServerPositionUpdateInterval, 0.05f);
        const float RotFactor  = FMath::Clamp(TimeSinceLastPositionUpdate / RotWindow, 0.f, 1.f);
        const float RotDeltaYaw = FMath::FindDeltaAngleDegrees(
            LastReceivedRotation.Yaw, TargetReceivedRotation.Yaw);
        const float LerpedYaw = FMath::UnwindDegrees(
            LastReceivedRotation.Yaw + RotDeltaYaw * RotFactor);
        SetActorRotation(FRotator(0.f, LerpedYaw, 0.f));
    }

    // -----------------------------------------------------------------------
    // 3. Animation speed / direction — EMA smoothing + graceful stop fade-out
    //
    //    Problems solved:
    //      a) Raw RemoteSpeed jumps instantly from 0 → full on first packet
    //         and stays at full until the 3× timeout → causes abrupt start/stop.
    //      b) RemoteDirection flips instantly between packets → blend-space pops.
    //
    //    Solution:
    //      • Track idle time so we know when the server has stopped sending
    //        meaningful displacement packets.
    //      • EMA-smooth speed toward the raw target value each tick.
    //      • When idle, fade SmoothedRemoteSpeed to 0 with a short decay so the
    //        walk→idle transition in the anim graph is driven by a curve, not
    //        a hard zero.
    //      • Direction is smoothed similarly so the blend-space never pops.
    // -----------------------------------------------------------------------

    // Grace period: one full server interval beyond the update window before we
    // consider the player idle.  This absorbs normal delivery jitter (~10-20 ms)
    // without introducing visible lag.
    const float GraceWindow = FMath::Max(ServerPositionUpdateInterval, 0.05f);
    const float GracePeriod = GraceWindow * 1.5f;

    // Idle clock runs whenever there is no displacement (packets reset it) —
    // including airborne-but-still vertical jumps that never set the moving
    // flag, so the landing rule below also fires for those.
    RemoteIdleTime += DeltaTime;
    if (bRemoteIsMoving)
    {
        if (RemoteIdleTime >= GracePeriod)
        {
            // No displacement in the last packet — begin fade-out.
            bRemoteIsMoving = false;
        }
    }

    // Movement interrupt: notify EVERY tick while moving, not just on the
    // edge. An emote begun mid-motion (flag already set, no new edge) would
    // otherwise play forever. NotifyMovementStarted is idempotent.
    if (bRemoteIsMoving && EmoteComponent)
    {
        EmoteComponent->NotifyMovementStarted();
    }

    // FLIGHT-TRACE: rendered vertical speed + flag state per tick.
    {
        const float ZNow = GetActorLocation().Z;
        if (!bRemoteTraceZSeeded)
        {
            RemoteLastTraceZ = ZNow;
            bRemoteTraceZSeeded = true;
        }
        const float Vz = DeltaTime > 0.f ? (ZNow - RemoteLastTraceZ) / DeltaTime : 0.f;
        RemoteLastTraceZ = ZNow;
        UE_LOG(LogGameplay, Verbose, TEXT("[FLIGHT] tick air=%d moving=%d idle=%.2f grace=%.2f actorZ=%.1f targetZ=%.1f Vz=%+.1f"),
            bRemoteIsInAir ? 1 : 0, bRemoteIsMoving ? 1 : 0, RemoteIdleTime, GracePeriod,
            ZNow, TargetReceivedPosition.Z, Vz);
    }

    // Landed-by-stillness: the airborne flag arrives only inside movement
    // packets, so a remote that lands and stands still would play fall
    // forever (until the next move/rotate packet). No displacement for a
    // full grace window means it is on the ground.
    if (FRemoteLandingRule::ShouldLand(bRemoteIsInAir, bRemoteIsMoving, RemoteIdleTime, GracePeriod))
    {
        bRemoteIsInAir = false;
        UE_LOG(LogGameplay, Verbose, TEXT("[FLIGHT] air 1->0 via stillness (idle=%.2f grace=%.2f)"),
            RemoteIdleTime, GracePeriod);
    }

    // Target values for this tick.
    const float TargetSpeed = bRemoteIsMoving ? RemoteSpeed : 0.0f;

    // EMA speeds: fast blend-in (0.2 weight on new) for startup,
    // slow blend-out (0.05 weight on new target=0) for graceful stop.
    // At 60 fps the fast path reaches ~98% of full speed in ~3 packets (~300 ms).
    // The slow path decays to near-zero in ~20 frames (~330 ms) — smooth fade.
    const float BlendIn  = FMath::Clamp(DeltaTime / 0.08f, 0.0f, 1.0f);  // ~80 ms rise
    const float BlendOut = FMath::Clamp(DeltaTime / 0.22f, 0.0f, 1.0f);  // ~220 ms fall
    const float SpeedAlpha = (TargetSpeed > SmoothedRemoteSpeed) ? BlendIn : BlendOut;

    SmoothedRemoteSpeed = FMath::Lerp(SmoothedRemoteSpeed, TargetSpeed, SpeedAlpha);

    // Snap to exactly zero to prevent the blend-space from hovering near 0
    // and flickering between idle and walk at very low smoothed values.
    // Direction fades toward 0 gradually instead of snapping, so the blend-space
    // settles into idle smoothly on both axes simultaneously — prevents the
    // backward-stop jerk where direction ±180° and speed both snap to 0 at once.
    if (SmoothedRemoteSpeed < 2.0f)
    {
        if (SmoothedRemoteSpeed < 0.5f)
        {
            // Truly negligible — hard-snap both to zero.
            SmoothedRemoteSpeed = 0.0f;
            SmoothedRemoteDirection = 0.0f;
        }
        else
        {
            // Speed is still decaying via the EMA above.
            // Fade direction toward neutral so the blend-space transitions
            // to idle without a direction-axis snap.
            const float DirFadeAlpha = FMath::Clamp(DeltaTime / 0.06f, 0.0f, 1.0f);
            const float DirDelta = FMath::FindDeltaAngleDegrees(SmoothedRemoteDirection, 0.0f);
            SmoothedRemoteDirection = FMath::UnwindDegrees(SmoothedRemoteDirection + DirDelta * DirFadeAlpha);
        }
    }

    // Direction: only blend when actually moving so we don't rotate the
    // blend-space axis while standing still (causes foot-sliding pop on restart).
    if (SmoothedRemoteSpeed > 2.0f)
    {
        const float DirAlpha = FMath::Clamp(DeltaTime / 0.10f, 0.0f, 1.0f);  // ~100 ms turn
        // Angle-aware interpolation: avoids the ±180° wrap-around issue where
        // FMath::Lerp on raw degrees crosses zero instead of staying near ±180,
        // causing the blend-space to sweep through all directions during
        // backward movement.
        const float DeltaAngle = FMath::FindDeltaAngleDegrees(SmoothedRemoteDirection, RemoteDirection);
        SmoothedRemoteDirection = FMath::UnwindDegrees(SmoothedRemoteDirection + DeltaAngle * DirAlpha);
    }
    // else: keep last known direction so the blend-space doesn't pop to 0 on stop.
}

    float ABasicPlayer::CalculateRotationInterpSpeed()
{
    float AngleDifference = FMath::Abs(LastReceivedRotation.Yaw - TargetReceivedRotation.Yaw);
    AngleDifference = FMath::Min(AngleDifference, 360.f - AngleDifference);
    return AngleDifference / ServerPositionUpdateInterval;
}

// ─────────────────────────────────────────────────────────────────────────────
// Character-select preview movement helpers
// ─────────────────────────────────────────────────────────────────────────────

void ABasicPlayer::SetPreviewAnimationSpeed(float Speed, float Direction)
{
    SmoothedRemoteSpeed     = Speed;
    SmoothedRemoteDirection = Direction;
    bRemoteIsMoving         = Speed > 1.f;
}

void ABasicPlayer::FinishPreviewMovement(const FVector& FinalPos, const FRotator& FinalRot)
{
    // Hard-place the actor at the final position.
    SetActorLocationAndRotation(FinalPos, FinalRot, false, nullptr, ETeleportType::TeleportPhysics);

    // Sync received-position state so UpdateRemotePlayerMovement stays put at FinalPos
    // once it is re-enabled by clearing bPreviewMovementActive.
    LastReceivedPosition        = FinalPos;
    TargetReceivedPosition      = FinalPos;
    LastReceivedRotation        = FinalRot;
    TargetReceivedRotation      = FinalRot;
    bHasReceivedFirstPosition   = true;
    TimeSinceLastPositionUpdate = 0.f;
    RemoteVelocity              = FVector::ZeroVector;
    RemoteSpeed                 = 0.f;
    SmoothedRemoteSpeed         = 0.f;
    RemoteDirection             = 0.f;
    SmoothedRemoteDirection     = 0.f;
    bRemoteIsMoving             = false;
    bPreviewMovementActive      = false;
}

// ─────────────────────────────────────────────────────────────────────────────

float ABasicPlayer::CalculateInterpolationSpeed(float MovementSpeed)
{
    float InterpolationSpeed = MovementSpeed * interpolationSpeedFactor;
    return FMath::Clamp(InterpolationSpeed, 1.0f, maxInterpolationSpeed);
}

