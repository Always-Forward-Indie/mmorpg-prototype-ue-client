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
