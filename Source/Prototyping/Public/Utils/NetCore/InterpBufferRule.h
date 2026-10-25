#pragma once

#include "CoreMinimal.h"

/**
 * Snapshot interpolation buffer for remote actors (pure logic, S10).
 *
 * Replaces open-loop dead reckoning (extrapolate past the latest packet,
 * overshoot under jitter, correct back) with render-delayed interpolation:
 * the game thread renders the position at (now - RenderDelay), strictly
 * between two buffered server snapshots. Under packet loss the buffer
 * drains and the caller falls back to dead reckoning.
 *
 * Snapshots use plain floats + double timestamps (no FVector/UWorld) so
 * Automation specs can pin the insert/sample/eviction contract.
 */
struct PROTOTYPING_API FInterpSnapshot
{
	double TimeSec = 0.0;
	float X = 0.0f;
	float Y = 0.0f;
	float Z = 0.0f;
	float Yaw = 0.0f;
};

struct PROTOTYPING_API FInterpBufferRule
{
	/** Ring capacity: 8 snapshots at 10Hz cover 0.8s (max render delay 0.25s + stalls). */
	static constexpr int32 Capacity = 8;

	/**
	 * Render delay from the measured server interval: one full interval plus
	 * jitter headroom, clamped to [0.1, 0.25]s. Pinned by spec.
	 */
	static double RenderDelaySec(double IntervalSec)
	{
		const double Raw = IntervalSec + 0.05;
		if (Raw < 0.1) return 0.1;
		if (Raw > 0.25) return 0.25;
		return Raw;
	}

	/**
	 * Insert keeping ascending time order. Late snapshots (older than the
	 * newest buffered) are dropped — they would bend the interpolation
	 * backwards. Oldest entries are evicted past Capacity.
	 * @return number of buffered snapshots after insert.
	 */
	static int32 InsertSorted(TArray<FInterpSnapshot>& Buf, const FInterpSnapshot& S)
	{
		if (Buf.Num() > 0 && S.TimeSec <= Buf.Last().TimeSec)
		{
			return Buf.Num();
		}
		Buf.Add(S);
		while (Buf.Num() > Capacity)
		{
			Buf.RemoveAt(0);
		}
		return Buf.Num();
	}

	/**
	 * Find the bracketing pair for RenderTime. Holds the newest snapshot when
	 * RenderTime runs past it (idle link), holds the oldest when before it.
	 * @return false when fewer than 2 snapshots are buffered (caller falls back).
	 */
	static bool Sample(const TArray<FInterpSnapshot>& Buf, double RenderTime, int32& OutA, int32& OutB, float& OutAlpha)
	{
		OutA = INDEX_NONE;
		OutB = INDEX_NONE;
		OutAlpha = 0.0f;
		if (Buf.Num() < 2)
		{
			return false;
		}
		if (RenderTime <= Buf[0].TimeSec)
		{
			OutA = 0;
			OutB = 0;
			return true;
		}
		const int32 Last = Buf.Num() - 1;
		if (RenderTime >= Buf[Last].TimeSec)
		{
			OutA = Last;
			OutB = Last;
			return true;
		}
		for (int32 i = 0; i < Last; ++i)
		{
			if (RenderTime >= Buf[i].TimeSec && RenderTime <= Buf[i + 1].TimeSec)
			{
				const double Span = Buf[i + 1].TimeSec - Buf[i].TimeSec;
				OutA = i;
				OutB = i + 1;
				OutAlpha = Span > 0.0 ? (float)((RenderTime - Buf[i].TimeSec) / Span) : 0.0f;
				return true;
			}
		}
		return false;
	}
};
