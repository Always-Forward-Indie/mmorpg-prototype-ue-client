#pragma once

#include "CoreMinimal.h"

/** Repair-all result classification (pure logic, no UObject). */
enum class ERepairAllOutcome : uint8
{
	/** At least one item repaired (full or partial — server stays authoritative). */
	Repaired,
	/** Success packet, but nothing repaired and nothing spent: never celebrate, never wipe. */
	NothingToRepair,
	/** Server reported an errorCode (e.g. insufficient_gold). */
	Failed
};

/**
 * Canonical owner of the repair-all outcome predicate used by
 * URepairShopWidget::HandleRepairAllResult. A hollow success (empty list,
 * zero spent) must never be shown as "All repaired" nor clear the shop list.
 * Pinned by MMO.Shop.RepairAll.
 */
struct PROTOTYPING_API FRepairAllOutcome
{
	static ERepairAllOutcome Classify(bool bHasError, int32 RepairedCount)
	{
		if (bHasError)
		{
			return ERepairAllOutcome::Failed;
		}
		if (RepairedCount > 0)
		{
			return ERepairAllOutcome::Repaired;
		}
		return ERepairAllOutcome::NothingToRepair;
	}
};
