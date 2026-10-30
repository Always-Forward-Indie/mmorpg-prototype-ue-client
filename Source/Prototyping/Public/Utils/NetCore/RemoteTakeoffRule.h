#pragma once

#include "CoreMinimal.h"

/**
 * Remote takeoff fusion (pure logic, floats/bools in / bool out).
 *
 * The airborne flag arrives only inside movement packets, so at jump start
 * the rendered actor already rises while the flag still reads ground. But a
 * symmetric vertical-speed check flaps: after touchdown the server flag
 * reads ground while interpolation still descends (~300ms lag), and every
 * tick re-asserts air until stillness clears it. Takeoff is therefore
 * corroborated, asymmetric:
 *   - rising: fast upward motion with the server target leading above the
 *     actor (jump/elevator start). Snap artifacts (huge Vz with the target
 *     already reached) are ignored;
 *   - falling: fast downward motion toward a below-target, and only when
 *     not fresh off an airborne spell (landing tail suppresses, cliff-edge
 *     starts pass). Threshold sits between stair climbing (~100 u/s) and
 *     jumps (~400-600 u/s). Pinned by MMO.Player.RemoteTakeoff.
 */
struct PROTOTYPING_API FRemoteTakeoffRule
{
	static float AirborneVzThreshold() { return 200.0f; }
	/** Minimum server-target lead (cm) proving motion intent, not a snap. */
	static float MinTargetLead() { return 5.0f; }
	/** Landing-tail suppression window (s) for falling takeoffs. */
	static float RecentAirWindow() { return 0.5f; }

	static bool ShouldTakeOffRising(float RenderedVz, float TargetLeadZ)
	{
		return RenderedVz > AirborneVzThreshold() && TargetLeadZ > MinTargetLead();
	}

	static bool ShouldTakeOffFalling(float RenderedVz, float TargetLeadZ, float TimeSinceAirborne)
	{
		return RenderedVz < -AirborneVzThreshold()
			&& TargetLeadZ < -MinTargetLead()
			&& TimeSinceAirborne > RecentAirWindow();
	}
};
