// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/Ambient/AmbientCreatureActor.h"
#include "Prototyping.h"
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
				UE_LOG(LogGameplay, Warning, TEXT("[AmbientCreature] '%s' — no Supported Agent with radius >= %.1f found, using CapsuleRadius as NavAgent."), *GetName(), CreatureDefinition->CapsuleRadius);
			}
			CMC->NavAgentProps.AgentRadius     = BestRadius;
			CMC->NavAgentProps.AgentHeight     = BestHeight;
			CMC->NavAgentProps.AgentStepHeight = -1.f;
			UE_LOG(LogGameplay, Verbose, TEXT("[AmbientCreature] '%s' — NavAgentProps set: radius=%.1f height=%.1f (CapsuleRadius=%.1f, clearance=%.1f)"),
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
		UE_LOG(LogGameplay, Error, TEXT("[AmbientCreature] '%s' — AIController NOT spawned. NavMesh movement will be skipped."), *GetName());
	}
	else
	{
		UE_LOG(LogGameplay, Verbose, TEXT("[AmbientCreature] '%s' — AIController spawned: %s"), *GetName(), *GetController()->GetClass()->GetName());
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
		UE_LOG(LogGameplay, Verbose, TEXT("[AmbientCreature] '%s' — duplicated %d behavior entries from ScheduleAsset '%s'"),
			*GetName(), RuntimeBehaviors.Num(), *ScheduleAsset->GetName());
	}
	else
	{
		UE_LOG(LogGameplay, Warning, TEXT("[AmbientCreature] '%s' — no ScheduleAsset assigned, creature will be static."), *GetName());
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

		UE_LOG(LogGameplay, Verbose, TEXT("[AmbientCreature] '%s' — schedule will start in %.1f seconds."), *GetName(), Delay);

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

