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

	/**
	 * Hi-res variant: RTT measured with sub-ms precision (FPlatformTime),
	 * server timestamps stay ms integers. Below the 2ms quantum floor the
	 * integer server processing cannot resolve a processing/RTT ratio, so
	 * only the ratio gate is relaxed there (processing beyond RTT + 1ms
	 * slop still fails as a clock issue). Every other bound matches
	 * IsValidSample. Used by UTimeSyncService on all networks; the ms
	 * variant above keeps its pinned integer-quantum semantics.
	 */
	static bool IsValidSampleHiRes(double RttMs, int64 T1, int64 T2,
		float MaxRTTMs = 1000.0f, float MaxProcessingMs = 100.0f, float MaxRatio = 0.8f)
	{
		// True-zero RTT is a clock anomaly, not a fast network.
		if (!(RttMs >= 0.001) || RttMs > MaxRTTMs)
		{
			return false;
		}

		const double ServerProcessing = static_cast<double>(T2 - T1);

		// Server processing bounds.
		if (ServerProcessing < 0.0 || ServerProcessing > MaxProcessingMs)
		{
			return false;
		}

		// Ratio gate only where it is measurable (RTT at/above quantum floor).
		if (RttMs >= 2.0 && ServerProcessing / RttMs > MaxRatio)
		{
			return false;
		}

		// Negative NTP latency beyond 1ms quantum slop indicates clock issues.
		if (RttMs + 1.0 < ServerProcessing)
		{
			return false;
		}

		return true;
	}

	/**
	 * Near-best filter: accept the sample when history is thin (< 3),
	 * else require it within Tolerance of the best (lowest) RTT over
	 * the trailing Window samples (best seeds from the new sample,
	 * exactly like the service loop). Pinned by MMO.TimeSync.NearBest.
	 */
	static bool IsNearBest(const TArray<float>& HistoryRTTs, float NewRTT,
		int32 MinHistory = 3, int32 Window = 10, float Tolerance = 1.2f)
	{
		if (HistoryRTTs.Num() < MinHistory)
		{
			return true;
		}
		float BestRTT = NewRTT;
		const int32 Start = FMath::Max(0, HistoryRTTs.Num() - Window);
		for (int32 i = Start; i < HistoryRTTs.Num(); ++i)
		{
			BestRTT = FMath::Min(BestRTT, HistoryRTTs[i]);
		}
		return NewRTT <= BestRTT * Tolerance;
	}
};
