#pragma once

#include "CoreMinimal.h"

/**
 * Inventory capacity gate (pure logic, no UObject/UWorld).
 *
 * Canonical owner of the predictive "can this grant land?" predicate.
 * The chunk server is authoritative (it refuses with errorCode inventory_full
 * and never mints a partial row); this rule only mirrors that decision
 * client-side for instant UX: toast + error sound without a wasted round trip.
 *
 * - maxSlots <= 0 means legacy/unlimited: every grant is предсказуемо accepted.
 * - Stacking (template already owned) never needs a new row: always accepted.
 * - Otherwise a new row needs a free slot: currentRows < maxSlots.
 * Pinned by MMO.Items.CapacityRule.
 */
struct PROTOTYPING_API FInventoryCapacityRule
{
	static int32 CountRows(int32 CurrentRows) { return CurrentRows < 0 ? 0 : CurrentRows; }

	static bool CanAcceptNewTemplate(int32 CurrentRows, int32 MaxSlots, bool bAlreadyOwnsTemplate)
	{
		if (bAlreadyOwnsTemplate) return true;
		if (MaxSlots <= 0) return true;
		return CountRows(CurrentRows) < MaxSlots;
	}

	static int32 FreeSlots(int32 CurrentRows, int32 MaxSlots)
	{
		if (MaxSlots <= 0) return INT32_MAX;
		return FMath::Max(0, MaxSlots - CountRows(CurrentRows));
	}

	static bool IsFull(int32 CurrentRows, int32 MaxSlots, bool bAlreadyOwnsTemplate)
	{
		return !CanAcceptNewTemplate(CurrentRows, MaxSlots, bAlreadyOwnsTemplate);
	}
};
