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

void AAmbientCreatureActor::MoveToLocation(const FVector& TargetLocation, float Acceptance, FSimpleDelegate OnArrived)
{
	PendingArrivalDelegate = OnArrived;

	AAIController* AIC = Cast<AAIController>(GetController());
	if (!AIC)
	{
		UE_LOG(LogGameplay, Warning, TEXT("[AmbientCreature] '%s' — MoveToLocation: no AIController, skipping movement."), *GetName());
		OnArrived.ExecuteIfBound();
		return;
	}

	UE_LOG(LogGameplay, Verbose, TEXT("[AmbientCreature] '%s' — MoveToLocation: target=(%.0f,%.0f,%.0f) acceptance=%.0f"),
		*GetName(), TargetLocation.X, TargetLocation.Y, TargetLocation.Z, Acceptance);

	// Log which NavMesh will be used for this move
#if ENABLE_DRAW_DEBUG
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		const FNavAgentProperties& AgentProps = GetNavAgentProps();
		ANavigationData* SelectedNavData = NavSys->GetNavDataForProps(AgentProps, GetActorLocation());
		if (SelectedNavData)
		{
			UE_LOG(LogGameplay, Verbose, TEXT("[AmbientCreature] '%s' — NavData selected: '%s' (AgentRadius=%.1f AgentHeight=%.1f)"),
				*GetName(), *SelectedNavData->GetName(),
				AgentProps.AgentRadius, AgentProps.AgentHeight);
		}
		else
		{
			UE_LOG(LogGameplay, Warning, TEXT("[AmbientCreature] '%s' — No NavData found for AgentRadius=%.1f AgentHeight=%.1f — using default!"),
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
		UE_LOG(LogGameplay, Warning, TEXT("[AmbientCreature] '%s' — MoveToLocation FAILED (no NavMesh or unreachable). Add NavMeshBoundsVolume to this level!"), *GetName());
		PendingArrivalDelegate.ExecuteIfBound();
		PendingArrivalDelegate.Unbind();
	}
	else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// Already at destination — unbind and complete immediately
		AIC->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
		UE_LOG(LogGameplay, Verbose, TEXT("[AmbientCreature] '%s' — MoveToLocation: already at goal."), *GetName());
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

	UE_LOG(LogGameplay, Verbose, TEXT("[AmbientCreature] '%s' — OnMoveCompleted: success=%s"),
		*GetName(), Result.IsSuccess() ? TEXT("true") : TEXT("false"));

	PendingArrivalDelegate.ExecuteIfBound();
	PendingArrivalDelegate.Unbind();
}

// ─────────────────────────────────────────────────────────────────────────────
// Idle Animation Cycle (parallel, same pattern as ABasicNPC)
// ─────────────────────────────────────────────────────────────────────────────

