#pragma once

#include "CoreMinimal.h"

/**
 * Proximity-query predicates for registry-backed actor lookups (pure logic).
 *
 * Canonical owner of the "nearest within range" contract used by the
 * GetNearest / GetAllInRange helpers (Inventory/Harvest/NPC managers):
 * iteration happens over the manager registries (O(registered actors)),
 * never via GetAllActorsOfClass (O(whole world)).
 *
 * Pure and dependency-free so Automation specs can pin it. Actor iteration
 * and FVector math stay in the managers/callers.
 */
struct PROTOTYPING_API FProximityRule
{
	/** True when a measured distance is within Radius (inclusive). */
	static bool IsInRange(float Distance, float Radius) { return Distance <= Radius; }

	/**
	 * Index of the smallest Distances[i] that is also within MaxDistance,
	 * or INDEX_NONE when no entry qualifies. Ties keep the first index.
	 */
	static int32 SelectNearestIndex(const TArray<float>& Distances, float MaxDistance)
	{
		int32 BestIndex = INDEX_NONE;
		float BestDist = MaxDistance + 1.0f;
		for (int32 i = 0; i < Distances.Num(); ++i)
		{
			const float D = Distances[i];
			if (D <= MaxDistance && D < BestDist)
			{
				BestDist = D;
				BestIndex = i;
			}
		}
		return BestIndex;
	}
};
