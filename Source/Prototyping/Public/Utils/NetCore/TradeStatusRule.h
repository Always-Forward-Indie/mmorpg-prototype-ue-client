#pragma once

#include "CoreMinimal.h"

/**
 * P2P trade session status (pure logic).
 *
 * Canonical owner of the session predicates used by UTradeManager
 * (IsInTrade, terminal-event cleanup) and UTradeWidget (confirm button):
 * a session is the non-empty server session id; confirm is allowed while
 * the session lives and WE have not confirmed yet (the peer's flag never
 * gates us); only cancelled/completed wipe the local snapshot — a decline
 * intentionally preserves it (the server error echo heals it via cancel).
 */
struct PROTOTYPING_API FTradeStatusRule
{
	/** A session is live while the server session id is non-empty. */
	static bool HasActiveSession(const FString& SessionId) { return !SessionId.IsEmpty(); }

	/** Confirm button: session live and we have not confirmed yet. */
	static bool CanConfirm(const FString& SessionId, bool bMyConfirmed)
	{
		return HasActiveSession(SessionId) && !bMyConfirmed;
	}

	/** Terminal events that wipe the local session snapshot (cancelled/completed only). */
	static bool ShouldClearSession(const FString& EventType)
	{
		return EventType.Equals(TEXT("tradeCancelled"), ESearchCase::IgnoreCase)
			|| EventType.Equals(TEXT("tradeComplete"), ESearchCase::IgnoreCase);
	}
};
