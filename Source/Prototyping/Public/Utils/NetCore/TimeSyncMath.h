#pragma once

#include "CoreMinimal.h"

/**
 * Pure NTP math for client<->server time sync (t0..t3).
 *
 * Canonical implementation used by UTimeSyncService and Automation specs.
 * Dependency-free (integers only): no UWorld, no UObject, no tunables.
 *
 * Formulas (double precision, rounded once at the end — never truncate):
 *   offset  = ((t1 - t0) + (t2 - t3)) / 2
 *   latency = ((t3 - t0) - (t2 - t1)) / 2
 */
struct PROTOTYPING_API FTimeSyncMath
{
	/** t0 sent, t1 server-recv, t2 server-send, t3 received. */
	static int64 OffsetMs(int64 T0, int64 T1, int64 T2, int64 T3);

	/** t0 sent, t1 server-recv, t2 server-send, t3 received. */
	static float LatencyMs(int64 T0, int64 T1, int64 T2, int64 T3);
};
