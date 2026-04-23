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
#include "Gameplay/Interaction/IWorldInteractable.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UCharacterPreviewManager::Initialize(UMyGameInstance* GI)
{
	GameInstanceRef = GI;
	HighlightedIndex = -1;
	bRotationEnabled = false;
	PreviewYawAccum = 0.0f;
}

void UCharacterPreviewManager::Cleanup()
{
	ClearSelectPreviews();
	ClearCreatePreview();

	// Remove the login-level nameplate canvas from the viewport
	if (IsValid(LoginNameplateCanvas))
	{
		LoginNameplateCanvas->RemoveFromParent();
		LoginNameplateCanvas = nullptr;
	}

	GameInstanceRef = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Character Select previews
// ─────────────────────────────────────────────────────────────────────────────

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

void UCharacterPreviewManager::UpdateCreatePreview(const FString& ClassSlug, const FString& RaceSlug, const FString& GenderName)
{
	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	// Use the first podium location for create preview
	const FVector SpawnPos = GameInstanceRef->CreatePreviewLocation;
	const FRotator SpawnRot = GameInstanceRef->CreatePreviewRotation;

	if (!CreatePreviewActor || !IsValid(CreatePreviewActor))
	{
		CreatePreviewActor = SpawnPreviewActor(FTransform(SpawnRot, SpawnPos));
	}

	if (CreatePreviewActor)
	{
		CreatePreviewActor->SetPlayerClass(ClassSlug);
		CreatePreviewActor->SetPlayerRace(RaceSlug);
		CreatePreviewActor->SetPlayerGender(GenderName);
		ApplyVisualToActor(CreatePreviewActor, ClassSlug, RaceSlug, GenderName);

		// Reset rotation accumulator when visual changes
		PreviewYawAccum = 0.0f;
	}
}

void UCharacterPreviewManager::ClearCreatePreview()
{
	if (IsValid(CreatePreviewActor) && !CreatePreviewActor->IsActorBeingDestroyed())
	{
		CreatePreviewActor->Destroy();
	}
	CreatePreviewActor = nullptr;
	bRotationEnabled = false;
	PreviewYawAccum = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera blending
// ─────────────────────────────────────────────────────────────────────────────

void UCharacterPreviewManager::BlendToSelectCamera(float BlendTime)
{
	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	APlayerController* PC = GameInstanceRef->GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	// If the login camera exists, use it as the blend target but move it to podium position
	if (GameInstanceRef->LoginLevelCamera)
	{
		GameInstanceRef->LoginLevelCamera->SetActorLocationAndRotation(
			GameInstanceRef->PodiumCameraLocation,
			GameInstanceRef->PodiumCameraRotation);

		PC->SetViewTargetWithBlend(GameInstanceRef->LoginLevelCamera, BlendTime, EViewTargetBlendFunction::VTBlend_EaseInOut);
	}
}

void UCharacterPreviewManager::BlendToCreateCamera(float BlendTime)
{
	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	APlayerController* PC = GameInstanceRef->GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	if (GameInstanceRef->LoginLevelCamera)
	{
		GameInstanceRef->LoginLevelCamera->SetActorLocationAndRotation(
			GameInstanceRef->CreatePreviewCameraLocation,
			GameInstanceRef->CreatePreviewCameraRotation);

		PC->SetViewTargetWithBlend(GameInstanceRef->LoginLevelCamera, BlendTime, EViewTargetBlendFunction::VTBlend_EaseInOut);
	}
}

void UCharacterPreviewManager::BlendToLoginCamera(float BlendTime)
{
	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	APlayerController* PC = GameInstanceRef->GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	if (GameInstanceRef->LoginLevelCamera)
	{
		GameInstanceRef->LoginLevelCamera->SetActorLocationAndRotation(
			GameInstanceRef->LoginLevelCameraLocation,
			GameInstanceRef->LoginLevelCameraRotation);

		PC->SetViewTargetWithBlend(GameInstanceRef->LoginLevelCamera, BlendTime, EViewTargetBlendFunction::VTBlend_EaseInOut);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Preview rotation (mouse drag)
// ─────────────────────────────────────────────────────────────────────────────

void UCharacterPreviewManager::TickPreviewRotation(float DeltaTime)
{
	if (!bRotationEnabled || !IsValid(CreatePreviewActor)) return;

	APlayerController* PC = GameInstanceRef ? GameInstanceRef->GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	float MouseX = 0.f, MouseY = 0.f;
	PC->GetInputMouseDelta(MouseX, MouseY);

	if (FMath::Abs(MouseX) > KINDA_SMALL_NUMBER && PC->IsInputKeyDown(EKeys::RightMouseButton))
	{
		PreviewYawAccum += MouseX * 0.5f;
		CreatePreviewActor->SetActorRotation(
			GameInstanceRef->CreatePreviewRotation + FRotator(0.0f, PreviewYawAccum, 0.0f));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

ABasicPlayer* UCharacterPreviewManager::SpawnPreviewActor(const FTransform& Transform)
{
	if (!GameInstanceRef || !GameInstanceRef->MainPlayerClass || !GameInstanceRef->GetWorld())
		return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABasicPlayer* Actor = GameInstanceRef->GetWorld()->SpawnActorDeferred<ABasicPlayer>(
		GameInstanceRef->MainPlayerClass, Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Actor) return nullptr;

	// Mark as "other client" so it doesn't create HUD/input bindings
	Actor->SetIsOtherClient(true);

	UGameplayStatics::FinishSpawningActor(Actor, Transform);

	// Keep capsule query-only so click line traces (ECC_Visibility) can hit it.
	// No physics response, no blocking of movement — purely for trace detection.
	if (UCapsuleComponent* Capsule = Actor->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// Disable movement completely — SetMovementMode(MOVE_None) stops gravity from
	// overriding our SetActorLocation calls. GravityScale=0 is belt-and-suspenders.
	if (UCharacterMovementComponent* MoveComp = Actor->GetCharacterMovement())
	{
		MoveComp->GravityScale = 0.f;
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	return Actor;
}

void UCharacterPreviewManager::ApplyVisualToActor(ABasicPlayer* Actor, const FString& ClassSlug, const FString& RaceSlug, const FString& GenderName)
{
	if (!Actor) return;

	UDataTable* Table = GetVisualTable();
	if (!Table)
	{
		UE_LOG(LogTemp, Warning, TEXT("CharacterPreviewManager: No CharacterVisualDefinitionsTable set on GameInstance"));
		return;
	}

	const FCharacterVisualDefinition* Def = CharacterVisualHelper::FindVisualDefinition(
		Table, ClassSlug, RaceSlug, GenderName);

	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("CharacterPreviewManager: No visual definition found for %s_%s_%s"),
			*ClassSlug, *RaceSlug, *GenderName);
		return;
	}

	const FCharacterVisualData& VisualData = Def->Visual;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	// Async load skeletal mesh
	TSoftObjectPtr<USkeletalMesh> SoftMesh = VisualData.SkeletalMesh;
	TSoftClassPtr<UAnimInstance> SoftAnimBP = VisualData.AnimBPClass;

	TWeakObjectPtr<ABasicPlayer> WeakActor(Actor);

	if (!SoftMesh.IsNull())
	{
		Streamable.RequestAsyncLoad(SoftMesh.ToSoftObjectPath(), [WeakActor, SoftMesh]()
		{
			if (!WeakActor.IsValid()) return;
			if (USkeletalMesh* LoadedMesh = SoftMesh.Get())
			{
				USkeletalMeshComponent* MeshComp = WeakActor->GetMesh();
				if (MeshComp)
				{
					MeshComp->SetSkeletalMesh(LoadedMesh);
					MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

					// Auto-fit capsule like mobs do
					const FBoxSphereBounds MeshBounds = LoadedMesh->GetBounds();
					const FVector BoxExtent = MeshBounds.BoxExtent;
					const FVector MeshOrigin = MeshBounds.Origin;

					UCapsuleComponent* Capsule = WeakActor->GetCapsuleComponent();
					if (Capsule)
					{
						float CapsuleHalfHeight = BoxExtent.Z;
						float MeshBottom = MeshOrigin.Z - BoxExtent.Z;
						float CapsuleBottom = -Capsule->GetUnscaledCapsuleHalfHeight();
						float MeshOffset = CapsuleBottom - MeshBottom;
						MeshComp->SetRelativeLocation(FVector(0, 0, MeshOffset));
						MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
					}
				}
			}
		});
	}

	if (!SoftAnimBP.IsNull())
	{
		Streamable.RequestAsyncLoad(SoftAnimBP.ToSoftObjectPath(), [WeakActor, SoftAnimBP]()
		{
			if (!WeakActor.IsValid()) return;
			if (UClass* AnimClass = SoftAnimBP.Get())
			{
				WeakActor->GetMesh()->SetAnimInstanceClass(AnimClass);
			}
		});
	}

	// Apply scale
	Actor->SetActorScale3D(VisualData.ActorScale);

	// Initialize default cosmetics (hair etc.) from the visual definition.
	// SetLeaderPoseComponent does not require the body mesh asset to be loaded yet.
	UCosmeticVisualComponent* CosmeticVis = Actor->GetCosmeticVisualComponent();
	UDataTable* CosmeticsTable = GameInstanceRef ? GameInstanceRef->CharacterCosmeticsDataTable : nullptr;
	UE_LOG(LogTemp, Log,
		TEXT("[Cosmetic] PreviewManager::ApplyVisualToActor — CosmeticVis=%s  GameInstanceRef=%s  CosmeticsTable=%s  HairSlug='%s'"),
		CosmeticVis ? TEXT("OK") : TEXT("NULL — CosmeticVisualComponent not found on actor (recompile + resave BP?)"),
		GameInstanceRef ? TEXT("OK") : TEXT("NULL"),
		CosmeticsTable ? *CosmeticsTable->GetName() : TEXT("NULL — assign CharacterCosmeticsDataTable in BP_GameInstance"),
		*Def->DefaultHairSlug.ToString());
	if (CosmeticVis)
	{
		CosmeticVis->SetDefaultCosmetics(*Def, CosmeticsTable);
	}
}

UDataTable* UCharacterPreviewManager::GetVisualTable() const
{
	return GameInstanceRef ? GameInstanceRef->CharacterVisualDefinitionsTable : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Equipment preview
// ─────────────────────────────────────────────────────────────────────────────

FString UCharacterPreviewManager::SlotIdToSlug(int32 SlotId)
{
	// Matches equip_slot table in DB (mmo_prototype_dump.sql)
	switch (SlotId)
	{
	case 1:  return TEXT("head");
	case 2:  return TEXT("chest");
	case 3:  return TEXT("legs");
	case 4:  return TEXT("feet");
	case 5:  return TEXT("hands");
	case 6:  return TEXT("main_hand");
	case 7:  return TEXT("off_hand");
	case 8:  return TEXT("two_hand");
	case 9:  return TEXT("ring");
	case 10: return TEXT("neck");
	case 11: return TEXT("trinket");
	default: return TEXT("");
	}
}

void UCharacterPreviewManager::ApplyEquipmentToActor(ABasicPlayer* Actor, const TArray<FLoginEquipmentEntry>& Equipment)
{
	if (!Actor || Equipment.IsEmpty()) return;

	UEquipmentVisualComponent* EquipVis = Actor->GetEquipmentVisualComponent();
	if (!EquipVis) return;

	UItemManager* ItemMgr = GameInstanceRef ? GameInstanceRef->GetItemManager() : nullptr;
	if (!ItemMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("CharacterPreviewManager: ItemManager not available — cannot apply equipment preview"));
		return;
	}

	EquipVis->InitializeForRemotePlayer(ItemMgr);

	// Build FEquipmentStateData from the lightweight preview entries
	FEquipmentStateData StateData;
	StateData.characterId = 0; // preview chars have no real ID

	for (const FLoginEquipmentEntry& Entry : Equipment)
	{
		const FString SlotSlug = SlotIdToSlug(Entry.SlotId);
		if (SlotSlug.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("CharacterPreviewManager: Unknown slotId %d for item '%s'"), Entry.SlotId, *Entry.ItemSlug);
			continue;
		}

		FEquipmentSlotData SlotData;
		SlotData.slotSlug   = SlotSlug;
		SlotData.itemSlug   = Entry.ItemSlug;
		SlotData.bIsOccupied = true;
		StateData.slots.Add(SlotSlug, SlotData);
	}

	EquipVis->RefreshAllSlots(StateData);

	// Pass the same equipment state to the cosmetic component so that
	// visibility rules are evaluated (e.g. helmet hides hair).
	if (UCosmeticVisualComponent* CosmeticVis = Actor->GetCosmeticVisualComponent())
	{
		CosmeticVis->ApplyEquipmentState(StateData);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Login-level nameplate canvas
// ─────────────────────────────────────────────────────────────────────────────

void UCharacterPreviewManager::EnsureLoginNameplateCanvas()
{
	if (IsValid(LoginNameplateCanvas) || !GameInstanceRef) return;
	if (!GameInstanceRef->LoginNameplateCanvasClass) return;

	UWorld* World = GameInstanceRef->GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, GameInstanceRef->LoginNameplateCanvasClass);
	LoginNameplateCanvas = Cast<UNameplateCanvasWidget>(Widget);
	if (LoginNameplateCanvas)
	{
		// Z-order 1 keeps nameplates behind the LoginFlowWidget (Z=10).
		LoginNameplateCanvas->AddToViewport(1);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Click-to-select
// ─────────────────────────────────────────────────────────────────────────────

int32 UCharacterPreviewManager::GetPreviewActorIndex(const ABasicPlayer* Actor) const
{
	for (int32 i = 0; i < SelectPreviewActors.Num(); ++i)
	{
		if (SelectPreviewActors[i] == Actor) return i;
	}
	return -1;
}
