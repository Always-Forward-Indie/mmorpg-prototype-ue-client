#pragma once

#include "CoreMinimal.h"

/**
 * Mid-session chunk-link predicate (pure logic, no sockets/threads).
 *
 * Canonical owner of the "link is down, start a bounded reconnect" decision
 * used by UNetworkManager::PollChunkServerNetworkData:
 * - IsLinkDown: the socket reports not-connected while a session exists.
 * - ShouldStartReconnect: session established + link down + no reconnect
 *   already running (the retry budget itself lives in FRetryPolicy).
 *
 * Pinned by MMO.Network.ChunkLinkRule.
 */
struct PROTOTYPING_API FChunkLinkRule
{
	/** True when the socket reports anything but a live connection. */
	static bool IsLinkDown(bool bSocketConnected) { return !bSocketConnected; }

	/** True when a mid-session reconnect should start now. */
	static bool ShouldStartReconnect(bool bSessionEstablished, bool bLinkDown, bool bReconnectInFlight)
	{
		return bSessionEstablished && bLinkDown && !bReconnectInFlight;
	}
};
