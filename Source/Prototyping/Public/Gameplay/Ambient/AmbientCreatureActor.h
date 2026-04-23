// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "AmbientCreatureActor.generated.h"

class UAmbientCreatureDefinition;
class UAmbientScheduleAsset;
class UAmbientBehaviorBase;
class USplineComponent;

/**
 * AAmbientCreatureActor
 *
 * A client-only ambient creature that executes a data-driven schedule of behaviors.
 * Place directly in a level — no Blueprint subclass required.
 * Assign CreatureDefinition (mesh/AnimBP/sounds) and ScheduleAsset (behavior list).
 *
 * The mesh updates live in the Viewport when you change CreatureDefinition (OnConstruction).
 *
 * bReplicates = false — zero network traffic. For login screen / decorative use only.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API AAmbientCreatureActor : public ACharacter
{
	GENERATED_BODY()

public:
	AAmbientCreatureActor();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

public:
	// ── Designer Properties ──────────────────────────────────────────────────

	/**
	 * Defines the visual appearance and audio of this creature.
	 * Changing this in the Details Panel will immediately update the mesh in the Viewport.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Creature")
	TObjectPtr<UAmbientCreatureDefinition> CreatureDefinition = nullptr;

	/**
	 * Defines the ordered list of behaviors this creature will execute.
	 * One DataAsset can be shared across multiple instances.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Creature")
	TObjectPtr<UAmbientScheduleAsset> ScheduleAsset = nullptr;

	/**
	 * Optional spline that defines a patrol path or flight route.
	 * Used by MoveToSplinePoint and FlyAlong behavior types.
	 * Select this component in the Viewport and drag the spline points to set the route.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient Creature")
	TObjectPtr<USplineComponent> PathSpline = nullptr;

	/**
	 * Draws debug visuals in PIE: movement target (yellow sphere), current path (green
	 * line segments), home location (cyan sphere), active behavior name.
	 * Visible in both attached and detached/eject mode.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Creature|Debug")
	bool bDebugDraw = false;

	// ── Runtime API (used by behavior classes) ───────────────────────────────

	/** The spawn-time world location. Used as the origin for random wander radii. */
	UFUNCTION(BlueprintCallable, Category = "Ambient Creature")
	FVector GetHomeLocation() const { return HomeLocation; }

	/** Returns the PathSpline component (may be nullptr if not needed). */
	UFUNCTION(BlueprintCallable, Category = "Ambient Creature")
	USplineComponent* GetPathSpline() const { return PathSpline; }

	/**
	 * Requests movement to TargetLocation via AIController / NavMesh.
	 * OnArrived fires when the destination is reached (or movement fails).
	 */
	void MoveToLocation(const FVector& TargetLocation, float Acceptance, FSimpleDelegate OnArrived);

	/** Stops any in-progress movement. */
	void StopMovement();

	/** Returns the NavAgentProperties configured for this creature (radius, height, agent name). */
	const FNavAgentProperties& GetNavAgentProps() const;

	/**
	 * Stops the parallel idle variant cycle so a behavior-driven montage
	 * can play without being interrupted.
	 */
	void InterruptIdleCycle();

	/** Resumes the idle variant cycle after a behavior-driven montage finishes. */
	void ResumeIdleCycle();

private:
	// ── Internal: Schedule Execution ─────────────────────────────────────────

	void StartSchedule();
	void ExecuteNextEntry();
	void OnEntryCompleted();
	int32 PickNextIndex() const;

	int32 CurrentEntryIndex = -1;
	UPROPERTY()
	TObjectPtr<UAmbientBehaviorBase> ActiveBehavior = nullptr;
	bool bScheduleRunning = false;

	/** Cached indices for random-order schedules to avoid repetition. */
	TArray<int32> ShuffledIndices;

	/**
	 * Per-instance copies of the DataAsset's behavior entries.
	 * Duplicated in BeginPlay so that multiple creatures sharing the same
	 * UAmbientScheduleAsset don't corrupt each other's runtime state.
	 * MUST be UPROPERTY so the GC doesn't collect the DuplicateObject copies.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UAmbientBehaviorBase>> RuntimeBehaviors;

	// ── Internal: Idle Cycle (parallel, from BasicNPC pattern) ───────────────

	void StartIdleAnimCycle();
	void ScheduleNextIdleVariant();
	void TriggerRandomIdleVariant();
	void OnIdleVariantMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void ScheduleNextIdleSound();
	void PlayRandomIdleSound();

	bool bIdleCycleActive = false;
	bool bIdleVariantPlaying = false;
	TObjectPtr<UAnimMontage> ActiveIdleMontage = nullptr;

	FOnMontageEnded IdleEndedDelegate;
	FTimerHandle IdleVariantTimerHandle;
	FTimerHandle IdleSoundTimerHandle;

	// ── Internal: Movement Callback ──────────────────────────────────────────

	void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

	FSimpleDelegate PendingArrivalDelegate;

	// ── Internal: LOD ────────────────────────────────────────────────────────

	void UpdateLOD();
	FTimerHandle LODTimerHandle;

	/** Deferred timer used to call ExecuteNextEntry on the next frame, preventing call-stack recursion. */
	FTimerHandle NextEntryTimerHandle;

	// ── Internal: Misc ───────────────────────────────────────────────────────

	FVector HomeLocation = FVector::ZeroVector;

	/** Audio component for idle sounds. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent = nullptr;
};
