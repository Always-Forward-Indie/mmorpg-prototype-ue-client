#pragma once

#include "CoreMinimal.h"

/**
 * Remote turn-rate policy (pure logic, floats in / float out).
 *
 * The raw per-packet heading jumps instantly (notably 0 <-> ±180 on
 * forward/backward reversal), while the smoothed blend-space axis must
 * follow without a sideways swim: small corrections turn slowly, full
 * reversals pivot in a couple of frames. Returns the lerp alpha for the
 * frame (0..1). Pinned by MMO.Player.RemoteTurn.
 */
struct PROTOTYPING_API FRemoteTurnRule
{
	static float TurnAlpha(float DeltaTime, float DeltaAngleDeg)
	{
		const float Rate = (FMath::Abs(DeltaAngleDeg) > 90.0f) ? 0.03f : 0.10f;
		return FMath::Clamp(DeltaTime / Rate, 0.0f, 1.0f);
	}
};
