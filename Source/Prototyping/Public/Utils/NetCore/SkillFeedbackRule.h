#pragma once

#include "CoreMinimal.h"

/**
 * Skill UI feedback rules (pure logic, no UObject/UWorld).
 *
 * Canonical owner of AAA deny/ready/start sound gating:
 * - Deny (cooldown / no-mana spam-click): throttled, never more often than MinDenyIntervalSec.
 * - Cooldown start blip: only for real cooldowns (>= MinStartCooldownSec), never for GCD-only blips
 *   and never for login-restore bursts (caller suppresses restore via bIsRestore).
 * - Ready blip: only for real cooldowns (>= MinReadyCooldownSec) that were visible on the bar
 *   (caller gates bWasAssigned). Short CDs and GCD-only slots stay silent.
 * Pinned by MMO.Skills.FeedbackRule.
 */
struct PROTOTYPING_API FSkillFeedbackRule
{
	/** Min seconds between two deny/mana error sounds (spam-click guard). */
	static constexpr float MinDenyIntervalSec = 0.4f;

	/** Cooldowns shorter than this never play start/ready blips (GCD noise guard). */
	static constexpr float MinAudibleCooldownSec = 1.5f;

	/** Ready blip requires a slightly longer CD so 1-2s skills don't chirp constantly. */
	static constexpr float MinReadyCooldownSec = 2.0f;

	static bool CanPlayDeny(float NowSec, float LastPlaySec)
	{
		// First ever play (negative sentinel) always allowed.
		if (LastPlaySec < -900.0f) return true;
		return (NowSec - LastPlaySec) >= MinDenyIntervalSec;
	}

	static bool ShouldPlayStart(float CooldownTotalSec, bool bIsAssigned, bool bIsRestore)
	{
		if (bIsRestore) return false;
		if (!bIsAssigned) return false;
		return CooldownTotalSec >= MinAudibleCooldownSec;
	}

	static bool ShouldPlayReady(float CooldownTotalSec, bool bWasAssigned)
	{
		if (!bWasAssigned) return false;
		return CooldownTotalSec >= MinReadyCooldownSec;
	}
};
