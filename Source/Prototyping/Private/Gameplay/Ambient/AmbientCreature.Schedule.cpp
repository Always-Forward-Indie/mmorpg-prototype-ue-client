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

