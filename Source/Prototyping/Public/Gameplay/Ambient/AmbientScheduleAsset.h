// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AmbientScheduleAsset.generated.h"

class UAmbientBehaviorBase;

/**
 * UAmbientScheduleAsset
 *
 * Data Asset containing the ordered (or random) list of behaviors for one creature type.
 * Create one per behavior pattern (e.g. DA_Deer_ForestPath, DA_Rabbit_Meadow).
 *
 * Designer workflow:
 *   1. Right-click in Content Browser → Miscellaneous → Data Asset → UAmbientScheduleAsset.
 *   2. Open the asset and click + on the Entries array.
 *   3. From the dropdown, pick a behavior type (Wait, Move To Spline Point, etc.).
 *   4. Fill in the behavior's parameters.
 *   5. Assign to AAmbientCreatureActor::ScheduleAsset in the Details Panel on the level.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UAmbientScheduleAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * The list of behaviors to execute in order.
	 * Each element is an instanced UObject subclassing UAmbientBehaviorBase.
	 * Click + to add a new entry, then select the behavior type from the dropdown.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Schedule")
	TArray<TObjectPtr<UAmbientBehaviorBase>> Entries;

	/**
	 * When true, entries are executed in a random order instead of sequentially.
	 * Useful for creatures with no fixed route (e.g. rabbits).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule")
	bool bRandomOrder = false;

	/**
	 * When true, the schedule loops indefinitely after the last entry finishes.
	 * When false, the creature enters an idle state after completing the schedule once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule")
	bool bLoopSchedule = true;

	/**
	 * When placing multiple instances of the same creature type, this value provides
	 * a maximum random offset (in seconds) applied to each instance's initial schedule
	 * start time — preventing all copies from moving in perfect sync.
	 * Set to 0 to disable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Schedule", meta = (ClampMin = "0.0"))
	float RandomInitialOffset = 30.f;
};
