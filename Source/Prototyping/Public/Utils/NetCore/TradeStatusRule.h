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

	/** Initiator election for bot/social flows: the lower character id
	 * invites, the higher accepts. Deterministic — no coin flip, no extra
	 * channel. Unknown ids (<= 0, replication not settled) never initiate. */
	static bool ShouldInitiateTrade(int32 OwnCharId, int32 PeerCharId)
	{
		return OwnCharId > 0 && PeerCharId > 0 && OwnCharId < PeerCharId;
	}

	/** Accept/decline echo field: the server matches the pending invite on
	 * fromCharacterId carrying the INITIATOR's id (as string). Echoing our
	 * own id answers "no_pending_invite" — this exact mistake shipped in
	 * both the bot and the trade widget (fixed 2026-10-20, bug #118).
	 * Empty when there is no invite to answer (fail-closed upstream). */
	static FString AcceptEchoField(int32 InitiatorId)
	{
		return InitiatorId > 0 ? FString::FromInt(InitiatorId) : FString();
	}
};
