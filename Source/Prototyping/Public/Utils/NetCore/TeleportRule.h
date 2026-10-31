#pragma once

#include "CoreMinimal.h"

/**
 * Teleport / respawn transition policy (pure logic).
 *
 * Canonical owner of the client-side cover-transition contract used by
 * UTeleportManager: when a server-authoritative snap must be hidden behind
 * a camera fade, how long each phase lasts, what counts as a far teleport
 * (extra streaming settle), and which targets are sane enough to apply.
 *
 * Thresholds mirror the server: respawn invulnerability is 3s
 * (respawn.invuln_sec), remote hard-snap is 500cm (BasicPlayer::SetCoordinates).
 * Fade durations match the WoW/FFXIV-class cover feel: fast out, brief hold
 * at peak (the snap lands here, invisible), gentle in.
 *
 * Pure and dependency-free (floats/doubles only, no FVector/UWorld) so
 * Automation specs can pin it. Actor ops stay in ABasicPlayer/UTeleportManager.
 */
struct PROTOTYPING_API FTeleportRule
{
	/** Fade-out (transparent -> black) before the snap. */
	static constexpr float FadeOutSec = 0.22f;
	/** Hold at full black while the snap + streaming settle lands. */
	static constexpr float HoldSec = 0.15f;
	/** Fade-in (black -> transparent) after the reveal. */
	static constexpr float FadeInSec = 0.30f;

	/** Full cover time from fade start to input unlock. */
	static float CoverTotalSec() { return FadeOutSec + HoldSec + FadeInSec; }

	/** Any snap at/above this distance is covered by a fade (never a bare pop). */
	static constexpr float CoverThresholdCm = 500.0f;
	/** At/above this distance the hold is extended for WorldPartition streaming. */
	static constexpr float FarTeleportCm = 5000.0f;
	/** Extra black-hold for far teleports so cells can stream in. */
	static constexpr float FarTeleportExtraHoldSec = 0.35f;

	/** Post-reveal tail: a far correction inside this window after the
	 *  reveal is stale pre-teleport history and is ignored. */
	static constexpr float CorrectionTailSec = 1.0f;

	/** Mirrors the server respawn.invuln_sec default (client never grants it). */
	static constexpr float SpawnProtectionSec = 3.0f;
	/** Give up waiting for the reveal timers and force-unlock input. */
	static constexpr float RevealSafetySec = 5.0f;

	/** Absolute sanity bound per axis (50km — anything past it is corrupt). */
	static constexpr double MaxReasonableCoord = 5000000.0;

	/** True when every coordinate is finite (no NaN/Inf from a corrupt packet). */
	static bool AreCoordsFinite(double X, double Y, double Z, double Yaw)
	{
		return FMath::IsFinite(X) && FMath::IsFinite(Y) && FMath::IsFinite(Z) && FMath::IsFinite(Yaw);
	}

	/** True when the authoritative target is sane enough to snap to. */
	static bool IsValidTarget(double X, double Y, double Z, double Yaw)
	{
		if (!AreCoordsFinite(X, Y, Z, Yaw))
		{
			return false;
		}
		return FMath::Abs(X) <= MaxReasonableCoord
			&& FMath::Abs(Y) <= MaxReasonableCoord
			&& FMath::Abs(Z) <= MaxReasonableCoord;
	}

	/** True when the displacement must be covered by a fade. */
	static bool NeedsCover(float DistCm) { return DistCm >= CoverThresholdCm; }

	/** True when the displacement needs the extended streaming hold. */
	static bool IsFarTeleport(float DistCm) { return DistCm >= FarTeleportCm; }

	/** Hold duration for a displacement (base hold + far-teleport extra). */
	static float HoldForDistance(float DistCm)
	{
		return HoldSec + (IsFarTeleport(DistCm) ? FarTeleportExtraHoldSec : 0.0f);
	}

	/** Normalize a server yaw into [-180, 180]. */
	static float NormalizeYaw(float Yaw)
	{
		float N = FMath::Fmod(Yaw + 180.0f, 360.0f);
		if (N < 0.0f)
		{
			N += 360.0f;
		}
		return N - 180.0f;
	}
};
