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

	static bool IsInCone(float Dot) { return Dot > 0.97f; }
	static bool IsInCone(float Dot, float MinDot) { return Dot > MinDot; }

	static bool AcceptSoftTarget(float DistanceCm, float RangeCm, float Dot, float MinDot = 0.97f)
	{
		return IsInRange(DistanceCm, RangeCm) && IsInCone(Dot, MinDot);
	}

	static bool ShouldCullHeadPlate(float DistanceCm, float CullCm) { return DistanceCm > CullCm; }
};
