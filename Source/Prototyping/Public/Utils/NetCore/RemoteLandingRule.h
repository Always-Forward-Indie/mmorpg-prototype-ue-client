#pragma once

#include "CoreMinimal.h"

/**
 * Remote landing detection (pure logic, floats/bools in / bool out).
 *
 * A remote avatar's airborne flag arrives only inside movement packets, so a
 * remote that lands and stands still keeps playing the fall animation until
 * the next packet (move or rotate). When the avatar is flagged airborne but
 * the interpolation pipeline reports no movement for a full grace window,
 * it has landed. Pinned by MMO.Player.RemoteLanding.
 */
struct PROTOTYPING_API FRemoteLandingRule
{
	static bool ShouldLand(bool bAirborne, bool bMoving, float IdleSeconds, float GraceSeconds)
	{
		return bAirborne && !bMoving && IdleSeconds >= GraceSeconds;
	}
};
