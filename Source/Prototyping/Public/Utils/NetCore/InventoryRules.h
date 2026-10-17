#pragma once

#include "CoreMinimal.h"

/**
 * Inventory business rules (pure logic, no JSON/UObject).
 *
 * Canonical owner of rules applied while deserializing inventory items.
 * Dependency-free so Automation specs can pin them directly.
 */
struct PROTOTYPING_API FInventoryRules
{
	/**
	 * Durability fallback: an explicit 0 from the server means broken and is
	 * never masked. Falls back to Max only when nobody sent the field, the
	 * item is durable, and Max is sane (> 0).
	 */
	static int32 ResolveDurabilityCurrent(bool bDurabilitySent, int32 DurabilityCurrent, int32 DurabilityMax, bool bIsDurable)
	{
		if (!bDurabilitySent && DurabilityCurrent == 0 && DurabilityMax > 0 && bIsDurable)
		{
			return DurabilityMax;
		}
		return DurabilityCurrent;
	}
};
