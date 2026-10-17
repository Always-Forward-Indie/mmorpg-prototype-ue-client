#pragma once

#include "CoreMinimal.h"

/**
 * Inventory grid index math (pure logic, no widgets).
 *
 * Canonical owner of the row-major slot mapping used by
 * UInventoryWidget (index = Row * Columns + Column). A non-positive
 * column count can never produce a valid index (the original code
 * would divide by zero) — it yields -1 / (0, 0) instead.
 * Pinned by MMO.Inventory.SlotMath.
 */
struct PROTOTYPING_API FSlotMath
{
	static int32 IndexFromPosition(int32 Row, int32 Column, int32 Columns)
	{
		if (Columns <= 0) { return INDEX_NONE; }
		return Row * Columns + Column;
	}

	static void PositionFromIndex(int32 SlotIndex, int32 Columns, int32& OutRow, int32& OutColumn)
	{
		if (Columns <= 0)
		{
			OutRow = 0;
			OutColumn = 0;
			return;
		}
		OutRow = SlotIndex / Columns;
		OutColumn = SlotIndex % Columns;
	}
};
