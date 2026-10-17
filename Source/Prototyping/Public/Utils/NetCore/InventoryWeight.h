#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/** Player-facing encumbrance bucket (drives the weight text color). */
enum class EWeightStatus : uint8
{
	Ok,
	Heavy,
	Overweight
};

/**
 * Inventory weight rules (pure logic, no widgets).
 *
 * Canonical owner of the carried-weight sum (weight x quantity per row)
 * and the encumbrance thresholds used by UInventoryWidget: >=100% red,
 * >=80% yellow, otherwise white; unknown max (<= 0) never alarms.
 * Pinned by MMO.Inventory.Weight.
 */
struct PROTOTYPING_API FInventoryWeight
{
	static float Total(const TArray<FInventoryItemStruct>& Items)
	{
		float TotalWeight = 0.0f;
		for (const FInventoryItemStruct& Item : Items)
		{
			TotalWeight += Item.weight * Item.quantity;
		}
		return TotalWeight;
	}

	static EWeightStatus StatusFor(float Current, float Max)
	{
		const float Ratio = Max > 0.0f ? (Current / Max) : 0.0f;
		if (Ratio >= 1.0f) { return EWeightStatus::Overweight; }
		if (Ratio >= 0.8f) { return EWeightStatus::Heavy; }
		return EWeightStatus::Ok;
	}
};
