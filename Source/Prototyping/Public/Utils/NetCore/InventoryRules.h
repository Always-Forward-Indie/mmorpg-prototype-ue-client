#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

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

	/**
	 * S2.3 delta merge (pure): apply server changed[]/removed[] onto the
	 * cached rows. Match by instance id; an id>0 row landing on a pending
	 * id==0 row of the same template is the id-sync case (update in place,
	 * never duplicate). Rows at quantity<=0 vanish (server deletes at 0).
	 * Full loads still replace wholesale; this only merges mutations.
	 */
	static void ApplyInventoryDelta(TArray<FInventoryItemStruct>& Items,
		const TArray<FInventoryItemStruct>& Changed, const TArray<int32>& RemovedIds)
	{
		for (int32 RemovedId : RemovedIds)
		{
			if (RemovedId > 0)
				Items.RemoveAll([RemovedId](const FInventoryItemStruct& It) { return It.id == RemovedId; });
		}
		for (const FInventoryItemStruct& Ch : Changed)
		{
			FInventoryItemStruct* Existing = nullptr;
			if (Ch.id > 0)
			{
				Existing = Items.FindByPredicate(
					[Id = Ch.id](const FInventoryItemStruct& It) { return It.id == Id; });
				if (!Existing)
				{
					Existing = Items.FindByPredicate(
						[ItemId = Ch.itemId](const FInventoryItemStruct& It) { return It.id == 0 && It.itemId == ItemId; });
				}
			}
			else
			{
				// Legacy id==0 rows match pending id==0 instances only:
				// never overwrite an assigned id with a placeholder.
				Existing = Items.FindByPredicate(
					[ItemId = Ch.itemId](const FInventoryItemStruct& It) { return It.id == 0 && It.itemId == ItemId; });
			}
			if (Existing)
			{
				*Existing = Ch;
			}
			else
			{
				Items.Add(Ch);
			}
		}
		Items.RemoveAll([](const FInventoryItemStruct& It) { return It.quantity <= 0; });
	}
};
