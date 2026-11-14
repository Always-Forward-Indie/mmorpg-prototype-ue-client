#pragma once

#include "CoreMinimal.h"

/**
 * Inventory grid capacity (pure logic, no widgets).
 *
 * Server maxSlots is authoritative for how many empty cells exist.
 * 0 = legacy/unlimited: keep the locally configured grid size.
 * Rows grow to fit the desired total; columns stay fixed so the
 * WrapBox layout never reflows sideways.
 * Consumed by UInventoryWidget::SyncGridToCapacity.
 * Pinned by MMO.UI.InventoryGrid.
 */
struct PROTOTYPING_API FInventoryGridRule
{
	static int32 DesiredTotal(int32 MaxSlots, int32 FallbackTotal)
	{
		if (MaxSlots > 0) return MaxSlots;
		return FMath::Max(1, FallbackTotal);
	}

	static int32 RowsForTotal(int32 Total, int32 Columns)
	{
		if (Columns <= 0) return FMath::Max(1, Total);
		return FMath::Max(1, (Total + Columns - 1) / Columns);
	}

	static bool NeedsResize(int32 CurrentTotal, int32 MaxSlots, int32 FallbackTotal)
	{
		return CurrentTotal != DesiredTotal(MaxSlots, FallbackTotal);
	}
};
