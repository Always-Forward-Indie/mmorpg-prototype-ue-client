#pragma once

#include "CoreMinimal.h"

/** Center-screen reticle state (crosshair feedback for the soft-target trace). */
UENUM(BlueprintType)
enum class ECrosshairState : uint8
{
	None		UMETA(DisplayName = "None"),
	Neutral		UMETA(DisplayName = "Neutral"),
	HoverMob	UMETA(DisplayName = "Hover Mob"),
	Locked		UMETA(DisplayName = "Locked"),
	OutOfRange	UMETA(DisplayName = "Out Of Range"),
	Dead		UMETA(DisplayName = "Dead Harvestable")
};

/**
 * Soft-target / nameplate range rule (pure logic, no UObject/UWorld).
 *
 * Canonical owner of the crosshair-trace acceptance predicate shared by
 * ABasicPlayer::CheckForMOB/CheckForNPC and Tab targeting:
 * - InRange: camera/player distance within the configured trace range.
 * - InCone: dot(cameraForward, toTarget) above the configured cone threshold.
 * - CullDistance is strictly larger than trace range: world head plates fade
 *   beyond trace range but the hard lock (target frame) survives until cleared.
 * Pinned by MMO.Targeting.RangeRule.
 */
struct PROTOTYPING_API FTargetingRule
{
	static bool IsInRange(float DistanceCm, float RangeCm) { return DistanceCm <= RangeCm; }

	static bool IsInCone(float Dot) { return Dot > 0.985f; }
	static bool IsInCone(float Dot, float MinDot) { return Dot > MinDot; }

	/** Zoom-independent trace length: gameplay range is measured from the
	 *  player, the ray itself travels camera->player plus that range, so
	 *  scrolling the spring arm never shrinks the capture zone. */
	static float EffectiveTraceLength(float CameraToPlayerCm, float GameplayRangeCm)
	{
		return FMath::Max(0.0f, CameraToPlayerCm) + FMath::Max(0.0f, GameplayRangeCm);
	}

	static bool AcceptSoftTarget(float DistanceCm, float RangeCm, float Dot, float MinDot = 0.97f)
	{
		return IsInRange(DistanceCm, RangeCm) && IsInCone(Dot, MinDot);
	}

	static bool ShouldCullHeadPlate(float DistanceCm, float CullCm) { return DistanceCm > CullCm; }

	/**
	 * Tab-cycle acceptance (pure): the candidate must sit ahead of the camera
	 * plane (MinDot = 0 selects the whole front hemisphere — "the direction
	 * the player looks") and have a confirmed line of sight (the trace itself
	 * lives in the producer; only its boolean result enters the rule).
	 * Range is checked separately via IsInRange.
	 */
	static bool IsInFront(float Dot, float MinDot = 0.0f) { return Dot > MinDot; }

	static bool AcceptTabTarget(float Dot, float MinDot, bool bHasLineOfSight)
	{
		return IsInFront(Dot, MinDot) && bHasLineOfSight;
	}

	/**
	 * Strict weak ordering for Tab cycling: smaller angle to the camera
	 * forward (larger dot) first — i.e. nearest to the crosshair — with
	 * squared distance as the tiebreak. The epsilon band keeps nearly-equal
	 * angles stable under float jitter.
	 */
	static bool TabCandidateLess(float DotA, float DistSqA, float DotB, float DistSqB, float Epsilon = 0.01f)
	{
		if (FMath::Abs(DotA - DotB) > Epsilon)
			return DotA > DotB;
		return DistSqA < DistSqB;
	}

	/**
	 * Reset-vs-cycle start index into the sorted visible Tab list:
	 * - locked target outside the visible set (stale/off-screen lock) -> 0,
	 *   the best target in the current view (never continue from an
	 *   off-screen lock);
	 * - otherwise advance one with wrap-around.
	 * Returns INDEX_NONE when the list is empty (caller clears the lock).
	 */
	static int32 TabStartIndex(int32 LockedSortedIndex, int32 NumCandidates)
	{
		if (NumCandidates <= 0) return INDEX_NONE;
		if (LockedSortedIndex < 0 || LockedSortedIndex >= NumCandidates) return 0;
		return (LockedSortedIndex + 1) % NumCandidates;
	}
};
