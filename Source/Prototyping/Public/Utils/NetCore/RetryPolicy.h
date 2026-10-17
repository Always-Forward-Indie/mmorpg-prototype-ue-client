#pragma once

#include "CoreMinimal.h"

/**
 * Connection-retry policy for the Login/Game/Chunk sockets (pure logic).
 *
 * Canonical owner of the retry contract used by UNetworkManager:
 * - a 1.0s poll tick checks the handshake; each failed tick bumps the counter;
 * - waiting continues while RetryCount <= MaxRetries, the run after that fails.
 *
 * Pure and dependency-free so Automation specs can pin it. Sockets, timers
 * and popups stay in UNetworkManager.
 */
struct PROTOTYPING_API FRetryPolicy
{
	/** Poll interval (seconds) between handshake checks. */
	static constexpr float ConnectionRetryIntervalSec = 1.0f;

	/** True while another wait tick is allowed (RetryCount <= MaxRetries). */
	static bool ShouldKeepWaiting(int32 RetryCount, int32 MaxRetries) { return RetryCount <= MaxRetries; }

	/** True once the budget is spent (RetryCount > MaxRetries) — fail the run. */
	static bool HasExceededMaxRetries(int32 RetryCount, int32 MaxRetries) { return RetryCount > MaxRetries; }
};
