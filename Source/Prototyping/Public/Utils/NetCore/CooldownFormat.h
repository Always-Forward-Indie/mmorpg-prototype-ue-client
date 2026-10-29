#pragma once

#include "CoreMinimal.h"

/**
 * Cooldown display formatting (pure logic, no widgets).
 *
 * Canonical owner of the skill-slot cooldown text rule used by
 * USkillSlotWidget: sub-second shows tenths, under a minute whole
 * seconds, beyond that m:ss. Pinned by MMO.UI.CooldownFormat.
 */
struct PROTOTYPING_API FCooldownFormat
{
	static FString Format(float Time)
	{
		if (Time < 1.0f)
		{
			return FString::Printf(TEXT("%.1f"), Time);
		}
		if (Time < 60.0f)
		{
			return FString::Printf(TEXT("%.0f"), Time);
		}
		const int32 Minutes = static_cast<int32>(Time / 60.0f);
		const int32 Seconds = static_cast<int32>(Time) % 60;
		return FString::Printf(TEXT("%d:%02d"), Minutes, Seconds);
	}
};

/**
 * Cooldown active flag (pure logic, no UObject).
 *
 * Canonical owner of the on-cooldown predicate used by
 * UPlayerSkillManager::UpdateSkillCooldownState: on cooldown while now
 * is strictly before the end time (equality = ready). Pinned by
 * MMO.Skills.CooldownState.
 */
struct PROTOTYPING_API FCooldownState
{
	static bool IsOnCooldown(float Now, float EndTime) { return Now < EndTime; }
};

/**
 * Travel re-anchor rule (pure logic, no UObject).
 *
 * Canonical owner of the world-clock reset rule used by
 * UPlayerSkillManager::ApplyTravelCooldownSnapshot: a remaining captured in
 * the old world is re-anchored to the new world's clock (which restarts near
 * zero, so raw old-world endTimes would read as stuck). Non-positive
 * remainings are dropped (already elapsed) instead of resurrecting a
 * cooldown; over-long remainings are clamped to the entry's own maximum so
 * clock garbage can never freeze a slot. Pinned by MMO.Skills.CooldownAnchor.
 */
struct PROTOTYPING_API FCooldownAnchor
{
	/** GCD carry cap: no legitimate GCD outlives a few seconds. Single source
	 *  for the display clamp in UPlayerSkillManager::GetGCDRemaining. */
	static constexpr double MaxGcdCarrySec = 10.0;

	/** Returns true + sets OutEndTime when the cooldown survives the travel;
	 *  returns false (OutEndTime untouched) when there is nothing to carry. */
	static bool Reanchor(double Now, double RemainingSec, double MaxRemainingSec, double& OutEndTime)
	{
		if (RemainingSec <= 0.0) { return false; }
		OutEndTime = Now + FMath::Min(RemainingSec, MaxRemainingSec);
		return true;
	}
};
