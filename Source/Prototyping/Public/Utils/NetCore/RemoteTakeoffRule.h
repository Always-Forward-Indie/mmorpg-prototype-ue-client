#pragma once

#include "CoreMinimal.h"

/**
 * Remote takeoff detection (pure logic, floats/bools in / bool out).
 *
 * The airborne flag arrives only inside movement packets, so at jump start
 * the rendered actor already rises while the flag still reads ground (one
 * or more packet intervals of wrong ground pose). Rendered vertical speed
 * closes that gap: fast vertical motion means airborne even when the flag
 * lags. Threshold sits between stair climbing (~100 u/s) and jumps
 * (~400-600 u/s). Pair with FRemoteLandingRule (which clears a stuck flag
 * after touchdown). Pinned by MMO.Player.RemoteTakeoff.
 */
struct PROTOTYPING_API FRemoteTakeoffRule
{
	static float AirborneVzThreshold() { return 200.0f; }

	static bool ShouldTakeOff(bool bFlagAirborne, float RenderedVz)
	{
		return !bFlagAirborne && FMath::Abs(RenderedVz) > AirborneVzThreshold();
	}
};
