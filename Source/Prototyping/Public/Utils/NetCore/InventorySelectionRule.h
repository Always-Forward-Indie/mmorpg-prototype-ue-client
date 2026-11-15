#pragma once

#include "CoreMinimal.h"

/**
 * Inventory LMB selection (pure logic, no widgets).
 *
 * AAA contract: plain LMB only selects (never uses/drops/equips).
 * Single-select + toggle: clicking the selected slot again clears it,
 * clicking an empty slot or an invalid index clears it as well.
 * Consumed by UInventoryWidget::OnSlotClicked (view-local grid index).
 * Pinned by MMO.UI.InventorySelection.
 */
struct PROTOTYPING_API FInventorySelectionRule
{
	/** Next selected grid index, or INDEX_NONE when selection clears. */
	static int32 Resolve(int32 CurrentSelected, int32 ClickedIndex, bool bClickedHasItem)
	{
		if (ClickedIndex < 0) return CurrentSelected;
		if (!bClickedHasItem) return INDEX_NONE;
		if (CurrentSelected == ClickedIndex) return INDEX_NONE;
		return ClickedIndex;
	}

	static bool IsValid(int32 Selected, int32 TotalSlots)
	{
		return Selected >= 0 && Selected < TotalSlots;
	}

	/**
	 * Double-click: second LMB on the already-selected slot inside the
	 * window. Pure time math (seconds, same clock both sides) so specs pin
	 * it with fake timestamps — no input dependency.
	 */
	static bool IsDoubleClick(int32 SelectedSlot, int32 LastSlot, double LastTime, double Now, double Window = 0.5)
	{
		if (SelectedSlot == INDEX_NONE || LastSlot == INDEX_NONE) return false;
		if (SelectedSlot != LastSlot) return false;
		if (Window <= 0.0) return false;
		const double Delta = Now - LastTime;
		return Delta >= 0.0 && Delta <= Window;
	}
};
