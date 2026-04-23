// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ambient/AmbientBehaviorBase.h"
#include "Animation/AnimMontage.h"
#include "AmbientBehaviors.generated.h"

class AAmbientCreatureActor;

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_Wait
// Pauses the schedule for a random duration in [MinDuration, MaxDuration].
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "Wait"))
class PROTOTYPING_API UAmbientBehavior_Wait : public UAmbientBehaviorBase
{
	GENERATED_BODY()

public:
	/** Minimum pause duration in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wait", meta = (ClampMin = "0.0"))
	float MinDuration = 2.f;

	/** Maximum pause duration in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wait", meta = (ClampMin = "0.0"))
	float MaxDuration = 6.f;

	virtual void Execute_Implementation(AAmbientCreatureActor* Owner) override;
	virtual bool IsComplete_Implementation(AAmbientCreatureActor* Owner) const override;
	virtual void OnAbort_Implementation(AAmbientCreatureActor* Owner) override;

private:
	float EndTime = 0.f;
	bool bStarted = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_MoveToSplinePoint
// Moves the creature to a specific point index on the actor's PathSpline.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "Move To Spline Point"))
class PROTOTYPING_API UAmbientBehavior_MoveToSplinePoint : public UAmbientBehaviorBase
{
	GENERATED_BODY()

public:
	/** Index into the PathSpline component's points array. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0"))
	int32 PointIndex = 0;

	/** How close (cm) the creature must get to consider the point reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "10.0"))
	float AcceptanceRadius = 80.f;

	virtual void Execute_Implementation(AAmbientCreatureActor* Owner) override;
	virtual bool IsComplete_Implementation(AAmbientCreatureActor* Owner) const override;
	virtual void OnAbort_Implementation(AAmbientCreatureActor* Owner) override;

private:
	bool bArrived = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_MoveToRandomPoint
// Moves the creature to a random NavMesh point within SearchRadius of HomeLocation.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "Move To Random Point"))
class PROTOTYPING_API UAmbientBehavior_MoveToRandomPoint : public UAmbientBehaviorBase
{
	GENERATED_BODY()

public:
	/** Radius around the creature's HomeLocation to search for a random destination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "50.0"))
	float SearchRadius = 600.f;

	/** How close (cm) the creature must get to consider the point reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "10.0"))
	float AcceptanceRadius = 80.f;

	virtual void Execute_Implementation(AAmbientCreatureActor* Owner) override;
	virtual bool IsComplete_Implementation(AAmbientCreatureActor* Owner) const override;
	virtual void OnAbort_Implementation(AAmbientCreatureActor* Owner) override;

private:
	bool bArrived = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_PlayMontage
// Plays an animation montage. Optionally waits for it to finish.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "Play Montage"))
class PROTOTYPING_API UAmbientBehavior_PlayMontage : public UAmbientBehaviorBase
{
	GENERATED_BODY()

public:
	/** The montage to play (e.g. AM_Deer_Graze, AM_Rabbit_Sniff). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/**
	 * When true: waits for the montage to finish naturally before moving to the next entry.
	 * When false: uses MinDuration/MaxDuration to determine how long to stay in this state
	 *             (montage loops or freerunning while the timer counts down).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage")
	bool bWaitForEnd = false;

	/** Used when bWaitForEnd=false. Minimum time to hold this behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage",
		meta = (ClampMin = "0.0", EditCondition = "!bWaitForEnd", EditConditionHides))
	float MinDuration = 4.f;

	/** Used when bWaitForEnd=false. Maximum time to hold this behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Montage",
		meta = (ClampMin = "0.0", EditCondition = "!bWaitForEnd", EditConditionHides))
	float MaxDuration = 10.f;

	virtual void Execute_Implementation(AAmbientCreatureActor* Owner) override;
	virtual bool IsComplete_Implementation(AAmbientCreatureActor* Owner) const override;
	virtual void OnAbort_Implementation(AAmbientCreatureActor* Owner) override;

private:
	bool bMontageEnded = false;
	float EndTime = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_WanderSteps
// Executes N random wander moves in sequence, pausing between each.
// Great for rabbits: gives natural stop-look-hop-stop rhythm.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "Wander Steps"))
class PROTOTYPING_API UAmbientBehavior_WanderSteps : public UAmbientBehaviorBase
{
	GENERATED_BODY()

public:
	/** Radius around HomeLocation to pick random destinations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander", meta = (ClampMin = "50.0"))
	float Radius = 600.f;

	/** Number of wander moves to perform before completing this entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander", meta = (ClampMin = "1"))
	int32 StepCount = 3;

	/** Minimum pause between steps (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander", meta = (ClampMin = "0.0"))
	float PauseMin = 0.5f;

	/** Maximum pause between steps (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander", meta = (ClampMin = "0.0"))
	float PauseMax = 2.f;

	/** How close (cm) the creature must get to each step destination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wander", meta = (ClampMin = "10.0"))
	float AcceptanceRadius = 80.f;

	virtual void Execute_Implementation(AAmbientCreatureActor* Owner) override;
	virtual bool IsComplete_Implementation(AAmbientCreatureActor* Owner) const override;
	virtual void OnAbort_Implementation(AAmbientCreatureActor* Owner) override;

private:
	int32 StepsRemaining = 0;
	bool bAllDone = false;
	FTimerHandle StepPauseTimerHandle;

	void StartNextStep(AAmbientCreatureActor* Owner);
};

// ─────────────────────────────────────────────────────────────────────────────
// UAmbientBehavior_FlyAlong
// Moves the creature along its PathSpline using direct SetActorLocation (no NavMesh).
// Gravity is disabled for the duration. Use this for birds and aerial creatures.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "Fly Along Spline"))
class PROTOTYPING_API UAmbientBehavior_FlyAlong : public UAmbientBehaviorBase
{
	GENERATED_BODY()

public:
	/** Movement speed along the spline in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fly", meta = (ClampMin = "10.0"))
	float FlySpeed = 600.f;

	/**
	 * Delay in seconds before the creature begins flying.
	 * Use this to desynchronize multiple birds placed in the same level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fly", meta = (ClampMin = "0.0"))
	float StartDelay = 0.f;

	/** When true the creature loops back silently to the spline start after reaching the end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fly")
	bool bLoopFlight = true;

	virtual void Execute_Implementation(AAmbientCreatureActor* Owner) override;
	virtual bool IsComplete_Implementation(AAmbientCreatureActor* Owner) const override;
	virtual void OnAbort_Implementation(AAmbientCreatureActor* Owner) override;

private:
	float CurrentDistance = 0.f;
	float DelayEndTime = 0.f;
	bool bDelaying = false;
	bool bFinished = false;
};
