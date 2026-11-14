#pragma once

#include "CoreMinimal.h"

/**
 * Mob target-frame presentation rules (pure logic, no widgets/UWorld).
 *
 * Canonical owner of every display predicate the target frame needs:
 * rank normalization + border colour, level-difference colour bucket with
 * the colourblind lethal marker, castbar visibility + server-time progress,
 * and target-effect ordering with the visible-slot cap.
 * Pinned by MMO.UI.MobTargetFrameRank / MMO.UI.MobTargetFrameCast /
 * MMO.UI.MobTargetFrameEffects.
 *
 * Forbidden here (client-owns-display boundary): gameplay arithmetic lives on
 * chunk (validate/spend/confirm). This file only maps already-decided facts
 * (rankCode, bIsDead, castTime, appliedEffects) to presentation values.
 */
struct PROTOTYPING_API FMobTargetRankRule
{
	/** Normalized rank codes. Empty = non-mob target (player/self) -> row hidden. */
	static FString NormalizeRankCode(const FString& RankCode, bool bIsChampion)
	{
		if (RankCode.IsEmpty())
		{
			return TEXT("");
		}
		if (bIsChampion || RankCode.Equals(TEXT("champion"), ESearchCase::IgnoreCase))
		{
			return TEXT("champion");
		}
		if (RankCode.Equals(TEXT("elite"), ESearchCase::IgnoreCase))
		{
			return TEXT("elite");
		}
		if (RankCode.Equals(TEXT("boss"), ESearchCase::IgnoreCase))
		{
			return TEXT("boss");
		}
		// Unknown codes degrade to normal — never hide a mob's rank row.
		return TEXT("normal");
	}

	/** Rank row is shown for every mob, including normal (never for players). */
	static bool ShouldShowRankLabel(const FString& NormalizedRank) { return !NormalizedRank.IsEmpty(); }

	/** Frame border colour per rank. Fixed (never localized), single source. */
	static FLinearColor BorderColorForRank(const FString& NormalizedRank)
	{
		if (NormalizedRank.Equals(TEXT("boss"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(1.0f, 0.45f, 0.10f, 1.0f);
		}
		if (NormalizedRank.Equals(TEXT("champion"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(0.70f, 0.30f, 1.0f, 1.0f);
		}
		if (NormalizedRank.Equals(TEXT("elite"), ESearchCase::IgnoreCase))
		{
			return FLinearColor(0.20f, 0.50f, 1.0f, 1.0f);
		}
		return FLinearColor(0.55f, 0.55f, 0.55f, 1.0f);
	}
};

/** Level-difference colour bucket (diff = viewerLevel - mobLevel). */
struct PROTOTYPING_API FMobTargetLevelRule
{
	struct FLevelStyle
	{
		FLinearColor Color = FLinearColor::White;
		/** Colourblind cue: lethal targets also carry a text marker, never colour alone. */
		bool bLethalMarker = false;
	};

	/** ASCII-only marker (no art/font dependency — guaranteed to render). */
	static FString LethalMarker() { return TEXT(" [!!]"); }

	static FLevelStyle StyleForDiff(int32 ViewerLevel, int32 MobLevel)
	{
		FLevelStyle Out;
		if (ViewerLevel <= 0)
		{
			return Out;
		}
		const int32 Diff = ViewerLevel - MobLevel;
		if (Diff <= -5)
		{
			Out.Color = FLinearColor(1.0f, 0.20f, 0.20f, 1.0f);
			Out.bLethalMarker = true;
		}
		else if (Diff <= -3)
		{
			Out.Color = FLinearColor(1.0f, 0.55f, 0.10f, 1.0f);
		}
		else if (Diff <= 2)
		{
			Out.Color = FLinearColor::White;
		}
		else if (Diff <= 5)
		{
			Out.Color = FLinearColor(0.30f, 1.0f, 0.30f, 1.0f);
		}
		else
		{
			Out.Color = FLinearColor(0.55f, 0.55f, 0.60f, 1.0f);
		}
		return Out;
	}
};

/** Target castbar: visibility gate + server-time progress (never local DeltaTime). */
struct PROTOTYPING_API FMobTargetCastRule
{
	/** Instant casts (castTime == 0) and the basic attack never show a bar. */
	static bool ShouldShowCastbar(float CastTimeSec, const FString& SkillSlug)
	{
		if (CastTimeSec <= 0.0f)
		{
			return false;
		}
		return !SkillSlug.Equals(TEXT("basic_attack"), ESearchCase::IgnoreCase);
	}

	/** 0..1 fill from the server clock: (serverNowMs - castStartedAtMs) / castTimeMs. */
	static float ProgressAt(int64 ServerNowMs, int64 CastStartedAtMs, float CastTimeSec)
	{
		if (CastTimeSec <= 0.0f)
		{
			return 1.0f;
		}
		const double ElapsedMs = static_cast<double>(ServerNowMs - CastStartedAtMs);
		return static_cast<float>(FMath::Clamp(ElapsedMs / (static_cast<double>(CastTimeSec) * 1000.0), 0.0, 1.0));
	}

	/** Seconds left on the server clock (floor at 0 for display). */
	static float RemainingSecAt(int64 ServerNowMs, int64 CastStartedAtMs, float CastTimeSec)
	{
		if (CastTimeSec <= 0.0f)
		{
			return 0.0f;
		}
		const double ElapsedSec = static_cast<double>(ServerNowMs - CastStartedAtMs) / 1000.0;
		return static_cast<float>(FMath::Max(0.0, static_cast<double>(CastTimeSec) - ElapsedSec));
	}
};

/** Target status-effects row: priority ordering + visible cap (pure data sort). */
struct PROTOTYPING_API FMobTargetEffectSort
{
	/** Max icon slots before the +N overflow label. */
	static constexpr int32 MaxVisibleEffects = 6;

	/** Shield / invulnerability / enrage first (matched by slug substring), then by time. */
	static int32 PriorityForSlug(const FString& EffectSlug)
	{
		if (EffectSlug.Contains(TEXT("shield"), ESearchCase::IgnoreCase))
		{
			return 0;
		}
		if (EffectSlug.Contains(TEXT("invuln"), ESearchCase::IgnoreCase)
			|| EffectSlug.Contains(TEXT("invulnerab"), ESearchCase::IgnoreCase)
			|| EffectSlug.Contains(TEXT("untargetable"), ESearchCase::IgnoreCase))
		{
			return 1;
		}
		if (EffectSlug.Contains(TEXT("enrage"), ESearchCase::IgnoreCase))
		{
			return 2;
		}
		return 3;
	}

	/** Strict weak ordering: priority first, then shortest remaining first. */
	static bool EffectLess(const FString& SlugA, float RemainingA, const FString& SlugB, float RemainingB)
	{
		const int32 PA = PriorityForSlug(SlugA);
		const int32 PB = PriorityForSlug(SlugB);
		if (PA != PB)
		{
			return PA < PB;
		}
		return RemainingA < RemainingB;
	}
};
