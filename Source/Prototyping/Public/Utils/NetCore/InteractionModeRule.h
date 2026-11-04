#pragma once

#include "CoreMinimal.h"
#include "Utils/NetCore/TargetingRule.h"

/**
 * Interaction mode rule (pure logic, no UObject/UWorld).
 *
 * Canonical owner of the BDO-style "cursor xor crosshair" decision.
 * The mouse cursor is the single source of truth (owned by UIManager):
 * cursor visible = UI/cursor mode (hover trace, decals, clicks own feedback,
 * center reticle stays hidden); cursor hidden = combat mode (camera-crosshair
 * trace owns feedback, reticle visible).
 * Pinned by MMO.Targeting.InteractionMode.
 */
UENUM(BlueprintType)
enum class EInteractionMode : uint8
{
	Cursor UMETA(DisplayName = "Cursor"),
	Combat UMETA(DisplayName = "Combat")
};

struct PROTOTYPING_API FInteractionModeRule
{
	static EInteractionMode Resolve(bool bCursorVisible)
	{
		return bCursorVisible ? EInteractionMode::Cursor : EInteractionMode::Combat;
	}

	/** Center reticle shows only in combat mode and only while alive. */
	static bool ShouldShowCrosshair(bool bCursorVisible, bool bIsDead)
	{
		if (bIsDead) return false;
		return Resolve(bCursorVisible) == EInteractionMode::Combat;
	}

	/** Shoulder target: constant offset in combat-inclusive design (no mode pops). */
	static FVector ResolveShoulderOffset(bool bApplyShoulder, const FVector& ConfigOffset)
	{
		return bApplyShoulder ? ConfigOffset : FVector::ZeroVector;
	}
};
