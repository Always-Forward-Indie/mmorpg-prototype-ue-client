// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AmbientBehaviorBase.generated.h"

class AAmbientCreatureActor;

/**
 * UAmbientBehaviorBase
 *
 * Abstract base for a single behavior step in a creature's schedule.
 * Subclass in C++ or Blueprint to create new behavior types.
 * All subclasses must be marked EditInlineNew so they can be instanced
 * directly inside UAmbientScheduleAsset's Entries array.
 *
 * Lifecycle per entry:
 *   1. Execute()   — called once when this entry becomes active
 *   2. IsComplete() — polled each tick; return true when done
 *   3. OnAbort()   — called if the schedule is interrupted mid-entry
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class PROTOTYPING_API UAmbientBehaviorBase : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Called once when this behavior becomes the active entry.
	 * Start timers, move requests, montage playback etc. here.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Ambient Behavior")
	void Execute(AAmbientCreatureActor* Owner);
	virtual void Execute_Implementation(AAmbientCreatureActor* Owner) {}

	/**
	 * Polled every schedule tick.
	 * Return true when the behavior has finished and the next entry can begin.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Ambient Behavior")
	bool IsComplete(AAmbientCreatureActor* Owner) const;
	virtual bool IsComplete_Implementation(AAmbientCreatureActor* Owner) const { return true; }

	/**
	 * Called if the schedule is forcibly interrupted (e.g. actor destroyed).
	 * Clean up any timers or ongoing requests.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Ambient Behavior")
	void OnAbort(AAmbientCreatureActor* Owner);
	virtual void OnAbort_Implementation(AAmbientCreatureActor* Owner) {}
};
