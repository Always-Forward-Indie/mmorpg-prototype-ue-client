// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/Ambient/AmbientCreatureActor.h"
#include "Gameplay/Ambient/AmbientCreatureDefinition.h"
#include "Gameplay/Ambient/AmbientScheduleAsset.h"
#include "Gameplay/Ambient/AmbientBehaviorBase.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "DrawDebugHelpers.h"

AAmbientCreatureActor::AAmbientCreatureActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 10 Hz — fine for ambient creatures

	// No server replication — this is a purely client-side cosmetic actor
	bReplicates = false;

	// Spline for patrol/flight routes — designer draws it in the Viewport
	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetupAttachment(RootComponent);

	// Audio for idle sounds
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
	AudioComponent->bAutoActivate = false;

	// Explicit AIController — required for NavMesh movement
	AIControllerClass = AAIController::StaticClass();

	// Reasonable defaults for a ground creature
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->bOrientRotationToMovement = true;
		CMC->RotationRate = FRotator(0.f, 270.f, 0.f);
		CMC->MaxWalkSpeed = 150.f;
	}
	bUseControllerRotationYaw = false;

	// Collide with world geometry and stand on the ground, but don't block pawns/projectiles.
	// "Pawn" profile blocks everything — we want WorldStatic/WorldDynamic collision for
	// proper ground detection and geometry avoidance, but invisible to gameplay queries.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionProfileName(TEXT("NoCollision"));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionObjectType(ECC_WorldDynamic);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic,  ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// OnConstruction — updates mesh/AnimBP live in the editor Viewport
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!CreatureDefinition) return;

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (CreatureDefinition->SkeletalMesh && MeshComp->GetSkeletalMeshAsset() != CreatureDefinition->SkeletalMesh)
		{
			MeshComp->SetSkeletalMesh(CreatureDefinition->SkeletalMesh);
		}
		if (CreatureDefinition->AnimBPClass && MeshComp->GetAnimClass() != CreatureDefinition->AnimBPClass)
		{
			MeshComp->SetAnimInstanceClass(CreatureDefinition->AnimBPClass);
		}
		MeshComp->SetRelativeLocation(CreatureDefinition->MeshRelativeOffset);
	}

	SetActorScale3D(CreatureDefinition->ActorScale);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(CreatureDefinition->CapsuleRadius, CreatureDefinition->CapsuleHalfHeight);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

	// Apply definition settings at runtime too (covers non-editor paths)
	if (CreatureDefinition)
	{
		// Resize capsule to match the species' physical size
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCapsuleSize(CreatureDefinition->CapsuleRadius, CreatureDefinition->CapsuleHalfHeight);
		}

		if (UCharacterMovementComponent* CMC = GetCharacterMovement())
		{
			CMC->MaxWalkSpeed = CreatureDefinition->WalkSpeed;
			CMC->RotationRate = FRotator(0.f, CreatureDefinition->RotationRate, 0.f);

			// Автоматически выбираем NavMesh с наименьшим AgentRadius >= CapsuleRadius.
			// Дизайнер задаёт агентов в Project Settings с нужным запасом (например
			// CapsuleRadius=30, а агент с radius=80 — запас 50 units от стен).
			// Код сам найдёт "самый плотный" агент, под который ещё влезает капсула.
			float BestRadius = -1.f;
			float BestHeight = CreatureDefinition->CapsuleHalfHeight * 2.f;
			if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
			{
				for (ANavigationData* NavData : NavSys->NavDataSet)
				{
					if (!NavData) continue;
					const FNavDataConfig& Cfg = NavData->GetConfig();
					if (Cfg.AgentRadius >= CreatureDefinition->CapsuleRadius)
					{
						if (BestRadius < 0.f || Cfg.AgentRadius < BestRadius)
						{
							BestRadius = Cfg.AgentRadius;
							BestHeight = Cfg.AgentHeight;
						}
					}
				}
			}
			if (BestRadius < 0.f)
			{
				// Нет подходящего агента — используем физический радиус как fallback
				BestRadius = CreatureDefinition->CapsuleRadius;
				UE_LOG(LogTemp, Warning, TEXT("[AmbientCreature] '%s' — no Supported Agent with radius >= %.1f found, using CapsuleRadius as NavAgent."), *GetName(), CreatureDefinition->CapsuleRadius);
			}
			CMC->NavAgentProps.AgentRadius     = BestRadius;
			CMC->NavAgentProps.AgentHeight     = BestHeight;
			CMC->NavAgentProps.AgentStepHeight = -1.f;
			UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — NavAgentProps set: radius=%.1f height=%.1f (CapsuleRadius=%.1f, clearance=%.1f)"),
				*GetName(), BestRadius, BestHeight, CreatureDefinition->CapsuleRadius, BestRadius - CreatureDefinition->CapsuleRadius);
		}
	}

	// Spawn and possess a basic AIController for NavMesh movement
	if (!GetController())
	{
		SpawnDefaultController();
	}

	if (!GetController())
	{
		UE_LOG(LogTemp, Error, TEXT("[AmbientCreature] '%s' — AIController NOT spawned. NavMesh movement will be skipped."), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — AIController spawned: %s"), *GetName(), *GetController()->GetClass()->GetName());
	}

	// Build per-instance runtime copies of behavior entries.
	// DataAsset entries are shared assets — we MUST duplicate them so that
	// multiple creatures using the same ScheduleAsset don't share mutable state.
	RuntimeBehaviors.Reset();
	if (ScheduleAsset)
	{
		for (UAmbientBehaviorBase* Entry : ScheduleAsset->Entries)
		{
			if (Entry)
			{
				UAmbientBehaviorBase* Copy = DuplicateObject<UAmbientBehaviorBase>(Entry, this);
				RuntimeBehaviors.Add(Copy);
			}
			else
			{
				RuntimeBehaviors.Add(nullptr);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — duplicated %d behavior entries from ScheduleAsset '%s'"),
			*GetName(), RuntimeBehaviors.Num(), *ScheduleAsset->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientCreature] '%s' — no ScheduleAsset assigned, creature will be static."), *GetName());
	}

	// Start parallel idle animation cycle
	StartIdleAnimCycle();

	// Schedule first idle sound
	ScheduleNextIdleSound();

	// Start the behavior schedule with an optional initial random offset
	if (RuntimeBehaviors.Num() > 0)
	{
		float Delay = 0.f;
		if (ScheduleAsset && ScheduleAsset->RandomInitialOffset > 0.f)
		{
			Delay = FMath::FRandRange(0.f, ScheduleAsset->RandomInitialOffset);
		}

		UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — schedule will start in %.1f seconds."), *GetName(), Delay);

		if (Delay > 0.f)
		{
			FTimerHandle StartHandle;
			GetWorld()->GetTimerManager().SetTimer(StartHandle, this, &AAmbientCreatureActor::StartSchedule, Delay, false);
		}
		else
		{
			StartSchedule();
		}
	}

	// LOD update every 2 seconds
	GetWorld()->GetTimerManager().SetTimer(LODTimerHandle, this, &AAmbientCreatureActor::UpdateLOD, 2.f, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// EndPlay
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(IdleVariantTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(IdleSoundTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(LODTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(NextEntryTimerHandle);

	if (ActiveBehavior)
	{
		ActiveBehavior->OnAbort(this);
		ActiveBehavior = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — polls IsComplete on the active behavior
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bScheduleRunning && ActiveBehavior)
	{
		if (ActiveBehavior->IsComplete(this))
		{
			OnEntryCompleted();
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDebugDraw)
	{
		const FVector MyLoc = GetActorLocation();

		// Home location — cyan sphere
		DrawDebugSphere(GetWorld(), HomeLocation, 20.f, 8, FColor::Cyan, false, -1.f, 0, 2.f);

		// Active behavior name
		if (ActiveBehavior)
		{
			DrawDebugString(GetWorld(), MyLoc + FVector(0, 0, 120.f),
				ActiveBehavior->GetClass()->GetName(), nullptr, FColor::White, 0.f, true, 1.f);
		}

		// Current path from AIController
		if (const AAIController* AIC = Cast<AAIController>(GetController()))
		{
			if (const UPathFollowingComponent* PFC = AIC->GetPathFollowingComponent())
			{
				const FNavPathSharedPtr CurrentPath = PFC->GetPath();
				if (CurrentPath.IsValid())
				{
					const TArray<FNavPathPoint>& Points = CurrentPath->GetPathPoints();
					for (int32 i = 0; i + 1 < Points.Num(); ++i)
					{
						DrawDebugLine(GetWorld(), Points[i].Location, Points[i + 1].Location,
							FColor::Green, false, -1.f, 0, 3.f);
					}
					// Target point — yellow sphere
					if (Points.Num() > 0)
					{
						DrawDebugSphere(GetWorld(), Points.Last().Location, 24.f, 8,
							FColor::Yellow, false, -1.f, 0, 3.f);
					}
				}
			}
		}
	}
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Schedule Execution
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::StartSchedule()
{
	if (RuntimeBehaviors.Num() == 0) return;

	bScheduleRunning = true;

	// Build shuffled index list for random-order schedules
	if (ScheduleAsset && ScheduleAsset->bRandomOrder)
	{
		ShuffledIndices.Reset();
		for (int32 i = 0; i < RuntimeBehaviors.Num(); ++i) ShuffledIndices.Add(i);
		// Fisher-Yates shuffle
		for (int32 i = ShuffledIndices.Num() - 1; i > 0; --i)
		{
			const int32 j = FMath::RandRange(0, i);
			ShuffledIndices.Swap(i, j);
		}
		CurrentEntryIndex = 0;
	}
	else
	{
		CurrentEntryIndex = 0;
	}

	UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — StartSchedule, %d entries, random=%s"),
		*GetName(), RuntimeBehaviors.Num(),
		(ScheduleAsset && ScheduleAsset->bRandomOrder) ? TEXT("true") : TEXT("false"));

	ExecuteNextEntry();
}

void AAmbientCreatureActor::ExecuteNextEntry()
{
	if (RuntimeBehaviors.Num() == 0) return;

	const bool bRandom = ScheduleAsset && ScheduleAsset->bRandomOrder;
	const int32 EntryArrayIndex = bRandom
		? (ShuffledIndices.IsValidIndex(CurrentEntryIndex) ? ShuffledIndices[CurrentEntryIndex] : 0)
		: CurrentEntryIndex;

	if (!RuntimeBehaviors.IsValidIndex(EntryArrayIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientCreature] '%s' — EntryArrayIndex %d out of range (%d entries)."),
			*GetName(), EntryArrayIndex, RuntimeBehaviors.Num());
		return;
	}

	UAmbientBehaviorBase* Behavior = RuntimeBehaviors[EntryArrayIndex];
	if (!Behavior)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientCreature] '%s' — null behavior at index %d, skipping."), *GetName(), EntryArrayIndex);
		OnEntryCompleted();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — executing entry [%d] '%s'"),
		*GetName(), EntryArrayIndex, *Behavior->GetClass()->GetName());

	ActiveBehavior = Behavior;
	ActiveBehavior->Execute(this);
}

void AAmbientCreatureActor::OnEntryCompleted()
{
	ActiveBehavior = nullptr;

	const int32 TotalEntries = RuntimeBehaviors.Num();
	if (TotalEntries == 0) return;

	++CurrentEntryIndex;

	if (CurrentEntryIndex >= TotalEntries)
	{
		if (!ScheduleAsset || !ScheduleAsset->bLoopSchedule)
		{
			UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — schedule finished (no loop)."), *GetName());
			bScheduleRunning = false;
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — schedule looped."), *GetName());

		// Loop: rebuild shuffle if needed
		if (ScheduleAsset && ScheduleAsset->bRandomOrder)
		{
			ShuffledIndices.Reset();
			for (int32 i = 0; i < TotalEntries; ++i) ShuffledIndices.Add(i);
			for (int32 i = ShuffledIndices.Num() - 1; i > 0; --i)
			{
				const int32 j = FMath::RandRange(0, i);
				ShuffledIndices.Swap(i, j);
			}
		}
		CurrentEntryIndex = 0;
	}

	// Defer to next frame — prevents recursive freeze when behaviors complete instantly
	GetWorld()->GetTimerManager().SetTimer(
		NextEntryTimerHandle,
		this,
		&AAmbientCreatureActor::ExecuteNextEntry,
		0.05f,
		false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Movement API (used by behavior classes)
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::MoveToLocation(const FVector& TargetLocation, float Acceptance, FSimpleDelegate OnArrived)
{
	PendingArrivalDelegate = OnArrived;

	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmbientCreature] '%s' — MoveToLocation: no AIController, skipping movement."), *GetName());
		OnArrived.ExecuteIfBound();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — MoveToLocation: target=(%.0f,%.0f,%.0f) acceptance=%.0f"),
		*GetName(), TargetLocation.X, TargetLocation.Y, TargetLocation.Z, Acceptance);

	// Log which NavMesh will be used for this move
#if ENABLE_DRAW_DEBUG
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		const FNavAgentProperties& AgentProps = GetNavAgentProps();
		ANavigationData* SelectedNavData = NavSys->GetNavDataForProps(AgentProps, GetActorLocation());
		if (SelectedNavData)
		{
			UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — NavData selected: '%s' (AgentRadius=%.1f AgentHeight=%.1f)"),
				*GetName(), *SelectedNavData->GetName(),
				AgentProps.AgentRadius, AgentProps.AgentHeight);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[AmbientCreature] '%s' — No NavData found for AgentRadius=%.1f AgentHeight=%.1f — using default!"),
				*GetName(), AgentProps.AgentRadius, AgentProps.AgentHeight);
		}
	}
#endif

	AIC->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &AAmbientCreatureActor::OnMoveCompleted);
	const EPathFollowingRequestResult::Type Result = AIC->MoveToLocation(TargetLocation, Acceptance, true, true, false, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		// NavMesh missing or target unreachable — unbind and skip immediately
		AIC->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
		UE_LOG(LogTemp, Warning, TEXT("[AmbientCreature] '%s' — MoveToLocation FAILED (no NavMesh or unreachable). Add NavMeshBoundsVolume to this level!"), *GetName());
		PendingArrivalDelegate.ExecuteIfBound();
		PendingArrivalDelegate.Unbind();
	}
	else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// Already at destination — unbind and complete immediately
		AIC->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
		UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — MoveToLocation: already at goal."), *GetName());
		PendingArrivalDelegate.ExecuteIfBound();
		PendingArrivalDelegate.Unbind();
	}
}

void AAmbientCreatureActor::StopMovement()
{
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}
}

const FNavAgentProperties& AAmbientCreatureActor::GetNavAgentProps() const
{
	if (const UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		return CMC->GetNavAgentPropertiesRef();
	}
	return FNavAgentProperties::DefaultProperties;
}

void AAmbientCreatureActor::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	// Unbind to avoid repeated callbacks on next move
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	}

	UE_LOG(LogTemp, Log, TEXT("[AmbientCreature] '%s' — OnMoveCompleted: success=%s"),
		*GetName(), Result.IsSuccess() ? TEXT("true") : TEXT("false"));

	PendingArrivalDelegate.ExecuteIfBound();
	PendingArrivalDelegate.Unbind();
}

// ─────────────────────────────────────────────────────────────────────────────
// Idle Animation Cycle (parallel, same pattern as ABasicNPC)
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::StartIdleAnimCycle()
{
	if (!CreatureDefinition) return;

	bIdleCycleActive = true;

	// Play the default idle montage immediately if assigned
	if (CreatureDefinition->DefaultIdleMontage && !bIdleVariantPlaying)
	{
		const float Duration = PlayAnimMontage(CreatureDefinition->DefaultIdleMontage);
		if (Duration > 0.f)
		{
			ActiveIdleMontage = CreatureDefinition->DefaultIdleMontage;
			bIdleVariantPlaying = true;

			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				IdleEndedDelegate.BindUObject(this, &AAmbientCreatureActor::OnIdleVariantMontageEnded);
				AnimInst->Montage_SetEndDelegate(IdleEndedDelegate, CreatureDefinition->DefaultIdleMontage);
			}
		}
	}

	ScheduleNextIdleVariant();
}

void AAmbientCreatureActor::ScheduleNextIdleVariant()
{
	if (!GetWorld() || !CreatureDefinition) return;
	if (!bIdleCycleActive) return;

	const float Delay = FMath::FRandRange(
		CreatureDefinition->IdleVariantMinDelay,
		CreatureDefinition->IdleVariantMaxDelay);

	GetWorld()->GetTimerManager().SetTimer(
		IdleVariantTimerHandle,
		this,
		&AAmbientCreatureActor::TriggerRandomIdleVariant,
		Delay,
		false);
}

void AAmbientCreatureActor::TriggerRandomIdleVariant()
{
	if (!bIdleCycleActive || bIdleVariantPlaying) return;
	if (!CreatureDefinition || CreatureDefinition->IdleVariantMontages.Num() == 0)
	{
		ScheduleNextIdleVariant();
		return;
	}

	const int32 Idx = FMath::RandRange(0, CreatureDefinition->IdleVariantMontages.Num() - 1);
	UAnimMontage* Montage = CreatureDefinition->IdleVariantMontages[Idx];
	if (!Montage) { ScheduleNextIdleVariant(); return; }

	const float Duration = PlayAnimMontage(Montage);
	if (Duration > 0.f)
	{
		ActiveIdleMontage = Montage;
		bIdleVariantPlaying = true;

		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			IdleEndedDelegate.BindUObject(this, &AAmbientCreatureActor::OnIdleVariantMontageEnded);
			AnimInst->Montage_SetEndDelegate(IdleEndedDelegate, Montage);
		}
	}
	else
	{
		ScheduleNextIdleVariant();
	}
}

void AAmbientCreatureActor::OnIdleVariantMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (ActiveIdleMontage != Montage) return;
	ActiveIdleMontage = nullptr;
	bIdleVariantPlaying = false;

	if (!bInterrupted && bIdleCycleActive)
	{
		// Return to default idle if assigned, then schedule next variant
		if (CreatureDefinition && CreatureDefinition->DefaultIdleMontage)
		{
			PlayAnimMontage(CreatureDefinition->DefaultIdleMontage);
		}
		ScheduleNextIdleVariant();
	}
}

void AAmbientCreatureActor::InterruptIdleCycle()
{
	bIdleCycleActive = false;
	GetWorld()->GetTimerManager().ClearTimer(IdleVariantTimerHandle);

	if (bIdleVariantPlaying && ActiveIdleMontage)
	{
		StopAnimMontage(ActiveIdleMontage);
		ActiveIdleMontage = nullptr;
		bIdleVariantPlaying = false;
	}
}

void AAmbientCreatureActor::ResumeIdleCycle()
{
	bIdleCycleActive = true;
	StartIdleAnimCycle();
}

// ─────────────────────────────────────────────────────────────────────────────
// Idle Sound Cycle
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::ScheduleNextIdleSound()
{
	if (!GetWorld() || !CreatureDefinition || CreatureDefinition->IdleSounds.Num() == 0) return;

	const float Delay = FMath::FRandRange(
		CreatureDefinition->IdleSoundMinDelay,
		CreatureDefinition->IdleSoundMaxDelay);

	GetWorld()->GetTimerManager().SetTimer(
		IdleSoundTimerHandle,
		this,
		&AAmbientCreatureActor::PlayRandomIdleSound,
		Delay,
		false);
}

void AAmbientCreatureActor::PlayRandomIdleSound()
{
	if (!CreatureDefinition || CreatureDefinition->IdleSounds.Num() == 0) return;

	const int32 Idx = FMath::RandRange(0, CreatureDefinition->IdleSounds.Num() - 1);
	USoundBase* Sound = CreatureDefinition->IdleSounds[Idx];

	if (Sound && AudioComponent)
	{
		AudioComponent->SetSound(Sound);
		AudioComponent->Play();
	}

	ScheduleNextIdleSound();
}

// ─────────────────────────────────────────────────────────────────────────────
// LOD — reduce tick rate based on distance to player
// ─────────────────────────────────────────────────────────────────────────────

void AAmbientCreatureActor::UpdateLOD()
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->GetPawn()) return;

	const float Dist = FVector::Dist(GetActorLocation(), PC->GetPawn()->GetActorLocation());

	if (Dist > 10000.f)
	{
		SetActorTickEnabled(false);
	}
	else if (Dist > 5000.f)
	{
		SetActorTickEnabled(true);
		SetActorTickInterval(0.5f);
	}
	else
	{
		SetActorTickEnabled(true);
		SetActorTickInterval(0.1f);
	}
}
