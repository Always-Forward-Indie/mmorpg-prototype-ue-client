#pragma once

#include "CoreMinimal.h"

/**
 * Mob follow-transition policy (pure logic, floats in / float out).
 *
 * Combat → patrol transitions used to always restart a positional blend from
 * the client position toward the server point. For small gaps (short return
 * after a nearby kill, EVADING hold) the blend runs backward and reads as a
 * rollback snap. The blend only pays off above what the patrol settle logic
 * absorbs on its own (PatrolSnapThreshold). Pinned by MMO.Mob.Follow.
 */
struct PROTOTYPING_API FMobFollowRule
{
	// Blend toward the server point only when the gap is worth animating.
	// At or below the snap threshold the patrol settle absorbs the residual.
	static bool ShouldBlendOnTransition(float TransitionGap, float SnapThreshold)
	{
		return TransitionGap > SnapThreshold;
	}

	// Blend duration replays the gap at roughly server speed so the mob does
	// not fast-blend ("teleport") across long chase drifts.
	static float TransitionBlendTime(float TransitionGap, float SafeSpeed, float MinTime, float MaxTime)
	{
		const float Safe = FMath::Max(SafeSpeed, 1.0f);
		return FMath::Clamp(TransitionGap / Safe, MinTime, MaxTime);
	}

	// Freeze dead reckoning only after a sustained packet drought. The server
	// streams combat holds at 10 Hz, so anything below the cap is jitter/loss
	// and the mob must keep moving; freezing there reads as stop-start flicker.
	static bool ShouldFreeze(float TimeSinceLastPacket, float MaxTime)
	{
		return TimeSinceLastPacket >= MaxTime;
	}
};
