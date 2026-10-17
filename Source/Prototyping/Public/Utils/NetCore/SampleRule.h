#pragma once

#include "CoreMinimal.h"
#include "Utils/NetCore/TimeSyncMath.h"

/**
 * NTP sample validity gate (pure logic, no UObject).
 *
 * Canonical owner of the IsSampleValid predicate used by
 * UTimeSyncService: RTT bounds, server-processing bounds, non-negative
 * NTP latency, and processing/RTT ratio cap. Bounds come from the
 * service tunables (defaults: RTT 1000ms, processing 100ms, ratio 0.8).
 * Pinned by MMO.TimeSync.SampleRule.
 */
struct PROTOTYPING_API FSampleRule
{
	static bool IsValidSample(int64 T0, int64 T1, int64 T2, int64 T3,
		float MaxRTTMs = 1000.0f, float MaxProcessingMs = 100.0f, float MaxProcessingRatio = 0.8f)
	{
		const float RTT = static_cast<float>(T3 - T0);
		const float ServerProcessing = static_cast<float>(T2 - T1);

		// RTT bounds.
		if (RTT <= 0 || RTT > MaxRTTMs)
		{
			return false;
		}

		// Server processing bounds.
		if (ServerProcessing < 0 || ServerProcessing > MaxProcessingMs)
		{
			return false;
		}

		// Negative calculated latency indicates clock issues.
		if (FTimeSyncMath::LatencyMs(T0, T1, T2, T3) < 0)
		{
			return false;
		}

		// Overloaded server: processing dominates the round trip.
		if (ServerProcessing / RTT > MaxProcessingRatio)
		{
			return false;
		}

		return true;
	}
};
