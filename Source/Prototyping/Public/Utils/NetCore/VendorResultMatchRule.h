#pragma once

#include "CoreMinimal.h"

/**
 * Vendor result matching (pure logic, FString/bool in / bool out).
 *
 * Results bind to their request by requestId echo — never by snapshot
 * heuristics (pre-sale gold/quantity comparison). A result applies when it
 * answers the pending request and hasn't been applied yet:
 *   - echo empty (predates-echo server): apply once (applied flag), the
 *     absolute inventory snapshot converges the rest;
 *   - echo present: apply only on exact match, once.
 * Stale/duplicate results are dropped so double-credit is impossible by
 * construction. Pinned by MMO.Shop.ResultMatch.
 */
struct PROTOTYPING_API FVendorResultMatchRule
{
	static bool ShouldApply(const FString& PendingRequestId, const FString& ResultEcho, bool bAlreadyApplied)
	{
		if (bAlreadyApplied) return false;
		if (ResultEcho.IsEmpty()) return true;
		if (PendingRequestId.IsEmpty()) return false;
		return PendingRequestId == ResultEcho;
	}
};
