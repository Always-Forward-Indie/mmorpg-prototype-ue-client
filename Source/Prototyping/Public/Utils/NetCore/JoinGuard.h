#pragma once

#include "CoreMinimal.h"

/**
 * Double-join guard for JoinSelectedCharacterToGame (pure logic, no UObject).
 *
 * Canonical owner of the "join already in flight" predicate: the first
 * caller wins (TryEnter returns true and latches), every concurrent or
 * repeated call while latched is ignored (TryEnter returns false).
 * Reset() clears the latch when the join flow settles (travel started or
 * failed) so the next explicit join works again.
 *
 * UMyGameInstance delegates 1:1 — no forked copies of this predicate.
 * Pinned by MMO.Network.JoinGuard.
 */
struct PROTOTYPING_API FJoinGuard
{
	/** Try to start a join. True = this caller owns it; false = one is already in flight. */
	bool TryEnter()
	{
		if (bEntered)
		{
			return false;
		}
		bEntered = true;
		return true;
	}

	/** True while a join started by TryEnter has not been Reset. */
	bool IsEntered() const { return bEntered; }

	/** Clear the latch (travel started/failed, or explicit cancel). */
	void Reset() { bEntered = false; }

private:
	bool bEntered = false;
};
