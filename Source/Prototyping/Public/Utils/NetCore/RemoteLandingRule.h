#pragma once

#include "CoreMinimal.h"

/**
 * Remote landing detection (pure logic, floats/bools in / bool out).
 *
 * A remote avatar's airborne flag arrives only inside movement packets, so a
 * remote that lands and stands still would play fall forever (until the next
 * move/rotate packet). But inferring landing from displacement-idle races
 * the broadcast rate (~200ms gaps): the rule fires BETWEEN packets
 * mid-flight. Land only when three independent signals agree: the rendered
 * body is vertically still, it has converged onto the server target, and no
 * packet has arrived recently (a live flag is always trusted as-is).
 * Pinned by MMO.Player.RemoteLanding.
 */
struct PROTOTYPING_API FRemoteLandingRule
{
	/** No packets for this long: the flag is stale, inference allowed. */
	static float LandingQuietTimeout() { return 0.35f; }
	/** Rendered-vs-target convergence epsilon (cm). */
	static float SettleEpsCm() { return 5.0f; }

	static bool ShouldLand(bool bFlagAirborne, bool bMoving, float AbsVz, float VzThreshold,
		float AbsTargetDeltaZ, float TimeSincePacket)
	{
		if (!bFlagAirborne || bMoving) return false;
		// Visibly moving vertically, or not converged onto the server
		// target: still flying (or interpolating), trust the flag.
		if (AbsVz > VzThreshold) return false;
		if (AbsTargetDeltaZ > SettleEpsCm()) return false;
		// Fresh packets mean a live flag — never second-guess it.
		return TimeSincePacket >= LandingQuietTimeout();
	}
};
