#pragma once

#include "CoreMinimal.h"

/**
 * Remote idle-clock reset (pure logic, floats in / bool out).
 *
 * The idle clock drives both the movement fade-out and landing-by-stillness.
 * It must reset on ANY displacement — planar or vertical. Resetting on XY
 * only leaves vertical in-place jumps running the idle clock mid-flight, so
 * the landing rule fires mid-air and the fall flag flaps (visible shake).
 * Thresholds mirror the packet path (1cm). Pinned by MMO.Player.RemoteIdleReset.
 */
struct PROTOTYPING_API FRemoteIdleResetRule
{
	static bool ShouldResetIdle(float Dist2D, float DeltaZ)
	{
		return Dist2D > 1.0f || FMath::Abs(DeltaZ) > 1.0f;
	}
};
