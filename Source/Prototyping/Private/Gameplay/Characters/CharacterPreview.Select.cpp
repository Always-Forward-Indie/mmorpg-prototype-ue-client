// Character Preview Manager — Implementation

#include "Gameplay/Characters/CharacterPreviewManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Data/CharacterVisualData.h"
#include "Data/DataStructs.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/UI/NameplateCanvasWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"

void UCharacterPreviewManager::SpawnCharacterPreviews(const TArray<FLoginCharacterEntry>& Characters)
{
	ClearSelectPreviews();

	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	const int32 Count = FMath::Min(Characters.Num(), GameInstanceRef->PodiumSpawnLocations.Num());
	CachedPodiumTransforms.Reset();

	for (int32 i = 0; i < Count; ++i)
	{
		const FVector& SpawnPos = GameInstanceRef->PodiumSpawnLocations[i];
		const FRotator SpawnRot = GameInstanceRef->PodiumSpawnRotations.IsValidIndex(i)
			? GameInstanceRef->PodiumSpawnRotations[i]
			: GameInstanceRef->PodiumSpawnRotation;

		ABasicPlayer* Preview = SpawnPreviewActor(FTransform(SpawnRot, SpawnPos));
		if (!Preview) continue;

		CachedPodiumTransforms.Add(FTransform(SpawnRot, SpawnPos));

		// Set basic data for nameplate
		Preview->SetPlayerName(Characters[i].CharacterName);
		Preview->SetPlayerClass(Characters[i].CharacterClass);
		Preview->SetPlayerRace(Characters[i].CharacterRace);
		Preview->SetPlayerGender(Characters[i].CharacterGender);
		Preview->SetPlayerLevel(Characters[i].CharacterLevel);

		// Apply visual from DataTable
		ApplyVisualToActor(Preview, Characters[i].CharacterClass, Characters[i].CharacterRace, Characters[i].CharacterGender);

		// Apply server equipment preview
		ApplyEquipmentToActor(Preview, Characters[i].Equipment);

		// Attach nameplate via the login-level canvas
		EnsureLoginNameplateCanvas();
		if (LoginNameplateCanvas)
		{
			LoginNameplateCanvas->RegisterPlayer(Preview,
				Characters[i].CharacterName,
				Characters[i].CharacterClass,
				Characters[i].CharacterLevel,
				false, 0.0f);
		}

		SelectPreviewActors.Add(Preview);
	}

	HighlightedIndex = -1;
}

void UCharacterPreviewManager::HighlightCharacter(int32 Index)
{
	// Prevent double-call when NativeTick fires HighlightCharacter AND SetSelectedItem
	// triggers HandleCharacterItemSelectionChanged in the same frame.
	if (Index == HighlightedIndex) return;

	// Restore + hide decal on previously selected actor
	if (HighlightedIndex >= 0 && SelectPreviewActors.IsValidIndex(HighlightedIndex))
	{
		if (ABasicPlayer* Prev = SelectPreviewActors[HighlightedIndex])
		{
			if (UTargetDecalComponent* Decal = Prev->FindComponentByClass<UTargetDecalComponent>())
				Decal->ForceHide();
		}
		RestoreCharacterToSlot(HighlightedIndex);
	}

	HighlightedIndex = Index;

	// Move forward + show decal on newly selected actor
	if (HighlightedIndex >= 0 && SelectPreviewActors.IsValidIndex(HighlightedIndex))
	{
		if (ABasicPlayer* Current = SelectPreviewActors[HighlightedIndex])
		{
			if (UTargetDecalComponent* Decal = Current->FindComponentByClass<UTargetDecalComponent>())
			{
				UWorldInteractionConfig* Cfg = GameInstanceRef ? GameInstanceRef->WorldInteractionConfig : nullptr;
				Decal->Apply(ETargetDecalState::Locked, Cfg, EInteractableType::RemotePlayer);
			}
		}
		MoveCharacterToFocusSlot(HighlightedIndex);
	}
}

void UCharacterPreviewManager::ClearSelectPreviews()
{
	// Cancel all pending walk requests before destroying actors.
	PendingMoves.Empty();

	// Unregister all preview actors from the login-level nameplate canvas
	if (IsValid(LoginNameplateCanvas))
	{
		LoginNameplateCanvas->UnregisterAll();
	}

	for (ABasicPlayer* Actor : SelectPreviewActors)
	{
		if (IsValid(Actor) && !Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	SelectPreviewActors.Empty();
	CachedPodiumTransforms.Empty();
	HighlightedIndex = -1;
}

void UCharacterPreviewManager::MoveCharacterToFocusSlot(int32 Index)
{
	if (!GameInstanceRef) return;
	if (!SelectPreviewActors.IsValidIndex(Index)) return;
	ABasicPlayer* Actor = SelectPreviewActors[Index];
	if (!IsValid(Actor)) return;

	// Cancel any in-flight move for this actor.
	for (FPreviewMoveRequest& Req : PendingMoves)
	{
		if (Req.Actor == Actor) Req.bActive = false;
	}

	FPreviewMoveRequest Req;
	Req.Actor           = Actor;
	Req.TargetLocation  = GameInstanceRef->SelectedCharacterLocation;
	Req.ArrivalRotation = GameInstanceRef->SelectedCharacterRotation;
	Req.bActive         = true;
	Req.CurrentSpeed    = 0.f;
	PendingMoves.Add(Req);

	// Engage preview-movement mode so UpdateRemotePlayerMovement is bypassed.
	Actor->SetPreviewMovementActive(true);
}

void UCharacterPreviewManager::RestoreCharacterToSlot(int32 Index)
{
	if (!SelectPreviewActors.IsValidIndex(Index)) return;
	ABasicPlayer* Actor = SelectPreviewActors[Index];
	if (!IsValid(Actor)) return;

	if (!CachedPodiumTransforms.IsValidIndex(Index)) return;

	// Cancel any in-flight move for this actor.
	for (FPreviewMoveRequest& Req : PendingMoves)
	{
		if (Req.Actor == Actor) Req.bActive = false;
	}

	FPreviewMoveRequest Req;
	Req.Actor           = Actor;
	Req.TargetLocation  = CachedPodiumTransforms[Index].GetLocation();
	Req.ArrivalRotation = CachedPodiumTransforms[Index].GetRotation().Rotator();
	Req.bActive         = true;
	Req.CurrentSpeed    = 0.f;
	PendingMoves.Add(Req);

	Actor->SetPreviewMovementActive(true);
}

void UCharacterPreviewManager::TickCharacterMovements(float DeltaTime)
{
	// Rotation uses RInterpConstantTo (degrees / second) for a predictable, natural turn.
	constexpr float MaxTurnDegPerSec = 300.f;
	// Decel zone: characters slow down and rotate toward the final facing.
	constexpr float DecelDist        = 80.f;   // cm
	constexpr float ArrivalDist      = 5.f;    // cm — trigger arrived phase
	// Arc weight: how much the actor's own forward direction bleeds into the move
	// direction during travel. 0 = straight line, higher = more arc.
	constexpr float ArcWeight        = 0.28f;

	for (int32 i = PendingMoves.Num() - 1; i >= 0; --i)
	{
		FPreviewMoveRequest& Move = PendingMoves[i];

		if (!Move.bActive || !IsValid(Move.Actor))
		{
			PendingMoves.RemoveAt(i);
			continue;
		}

		// ── Phase 2: arrived — smooth final position + rotation + anim ramp ──
		if (Move.bArrived)
		{
			// Smoothly close the last gap in position (already ≤ ArrivalDist when triggered).
			const FVector CurLoc = Move.Actor->GetActorLocation();
			const FVector NewLoc = FMath::VInterpTo(CurLoc, Move.TargetLocation, DeltaTime, 10.f);
			Move.Actor->SetActorLocation(NewLoc, false, nullptr, ETeleportType::None);

			// Continue rotating toward the arrival facing at constant rate.
			const FRotator NewRot = FMath::RInterpConstantTo(
				Move.Actor->GetActorRotation(), Move.ArrivalRotation, DeltaTime, MaxTurnDegPerSec);
			Move.Actor->SetActorRotation(NewRot);

			// Decay anim speed with ~150 ms half-life.
			Move.CurrentSpeed = FMath::Lerp(Move.CurrentSpeed, 0.f,
				FMath::Clamp(DeltaTime / 0.15f, 0.f, 1.f));
			Move.Actor->SetPreviewAnimationSpeed(Move.CurrentSpeed, 0.f);

			// Commit only when both animation and position have fully settled.
			const float FinalDist = FVector::Dist2D(Move.Actor->GetActorLocation(), Move.TargetLocation);
			if (Move.CurrentSpeed < 1.5f && FinalDist < 2.f)
			{
				Move.Actor->FinishPreviewMovement(Move.TargetLocation, Move.ArrivalRotation);
				PendingMoves.RemoveAt(i);
			}
			continue;
		}

		// ── Phase 1: travelling ──────────────────────────────────────────────
		const FVector CurrentLoc  = Move.Actor->GetActorLocation();
		const FVector Diff2D      = FVector(Move.TargetLocation.X - CurrentLoc.X,
		                                    Move.TargetLocation.Y - CurrentLoc.Y, 0.f);
		const float   DistXY      = Diff2D.Size();
		const FVector TravelDir2D = Diff2D.GetSafeNormal();
		const float   TravelYaw   = FMath::RadiansToDegrees(
			FMath::Atan2(TravelDir2D.Y, TravelDir2D.X));
		const FRotator TravelRot(0.f, TravelYaw, 0.f);

		// Trigger arrival phase — no snap, let Phase 2 handle the final approach.
		if (DistXY < ArrivalDist)
		{
			Move.bArrived = true;
			continue;
		}

		// ── Rotation (constant deg/sec — predictable, not springy) ───────────
		FRotator DesiredRot;
		if (DistXY > DecelDist)
		{
			// Travel phase: always face the movement direction.
			DesiredRot = TravelRot;
		}
		else
		{
			// Decel zone: slerp from travel dir to arrival facing.
			const float T = 1.f - FMath::Clamp(
				(DistXY - ArrivalDist) / (DecelDist - ArrivalDist), 0.f, 1.f);
			DesiredRot = FQuat::Slerp(
				FQuat(TravelRot), FQuat(Move.ArrivalRotation), T).Rotator();
		}

		// RInterpConstantTo turns at a fixed angular velocity (degrees/sec),
		// so the arc feels the same regardless of frame rate.
		Move.Actor->SetActorRotation(FMath::RInterpConstantTo(
			Move.Actor->GetActorRotation(), DesiredRot, DeltaTime, MaxTurnDegPerSec));

		// ── Arc movement direction ───────────────────────────────────────────
		// During travel: blend straight-to-target with the actor's current forward.
		// This makes the character curve naturally while turning instead of
		// sliding sideways. ArcWeight is small so convergence is guaranteed.
		// In the decel zone: move straight to target for a precise landing.
		FVector MoveDir;
		if (DistXY > DecelDist)
		{
			const FVector ActorFwd2D = FVector(
				Move.Actor->GetActorForwardVector().X,
				Move.Actor->GetActorForwardVector().Y, 0.f).GetSafeNormal();
			// Scale arc by misalignment: full arc only when not yet facing target.
			const float Misalign  = FMath::Clamp(
				1.f - FVector::DotProduct(ActorFwd2D, TravelDir2D), 0.f, 1.f);
			MoveDir = FMath::Lerp(TravelDir2D, ActorFwd2D,
				ArcWeight * Misalign).GetSafeNormal();
		}
		else
		{
			MoveDir = TravelDir2D;
		}

		// ── Speed ramp ───────────────────────────────────────────────────────
		const float SpeedTarget = (DistXY > DecelDist)
			? PreviewWalkSpeed
			: PreviewWalkSpeed * FMath::Clamp(DistXY / DecelDist, 0.f, 1.f);

		// Ramp-up ~80 ms, ramp-down ~120 ms.
		const float RampAlpha = (SpeedTarget > Move.CurrentSpeed)
			? FMath::Clamp(DeltaTime / 0.08f, 0.f, 1.f)
			: FMath::Clamp(DeltaTime / 0.12f, 0.f, 1.f);
		Move.CurrentSpeed = FMath::Lerp(Move.CurrentSpeed, SpeedTarget, RampAlpha);

		// ── Position ─────────────────────────────────────────────────────────
		const float StepDist = FMath::Min(Move.CurrentSpeed * DeltaTime, DistXY);
		FVector NewLoc       = CurrentLoc + MoveDir * StepDist;
		NewLoc.Z = FMath::FInterpTo(CurrentLoc.Z, Move.TargetLocation.Z, DeltaTime, 4.f);
		Move.Actor->SetActorLocation(NewLoc, false, nullptr, ETeleportType::None);

		// ── Animation ────────────────────────────────────────────────────────
		Move.Actor->SetPreviewAnimationSpeed(Move.CurrentSpeed, 0.f);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Character Create preview
// ─────────────────────────────────────────────────────────────────────────────

