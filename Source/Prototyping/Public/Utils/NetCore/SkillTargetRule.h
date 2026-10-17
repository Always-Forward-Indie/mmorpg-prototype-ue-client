#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/** Machine-readable outcome of skill target resolution. */
enum class ETargetResolveOutcome : uint8
{
	OkSelf,
	OkExternal,
	OkFallback,
	MissingTarget,
	SelfTargetRejected,
	UnknownPolicy
};

/** Resolved target + outcome (inputs pass through untouched on failure). */
struct FSkillTargetResolution
{
	ETargetResolveOutcome Outcome = ETargetResolveOutcome::UnknownPolicy;
	int32 TargetId = 0;
	ECasterType TargetType = ECasterType::None;
};

/**
 * Skill target-policy derivation (pure logic, no UObject).
 *
 * Canonical owner of the Auto-policy mapping (effectType -> policy) used
 * by UPlayerSkillManager::GetEffectiveTargetPolicy: the manager keeps the
 * skill lookup + the designer's explicit-policy override, this struct owns
 * only the deterministic derivation table. Pinned by MMO.Skills.TargetPolicy.
 */
struct PROTOTYPING_API FSkillTargetRule
{
	static ESkillTargetPolicy PolicyForEffect(ESkillEffectType EffectType)
	{
		switch (EffectType)
		{
		case ESkillEffectType::Damage:
		case ESkillEffectType::Debuff:
			return ESkillTargetPolicy::TargetOnly;

		case ESkillEffectType::Healing:
		case ESkillEffectType::Buff:
		case ESkillEffectType::Resource:
			return ESkillTargetPolicy::SelfAndTarget;

		case ESkillEffectType::Teleport:
		case ESkillEffectType::None:
		default:
			return ESkillTargetPolicy::SelfOnly;
		}
	}

	static bool IsSelfCastable(ESkillTargetPolicy Policy)
	{
		return Policy == ESkillTargetPolicy::SelfOnly || Policy == ESkillTargetPolicy::SelfAndTarget;
	}

	/**
	 * Full target-resolution decision table (pure; no logs, no lookups).
	 * The manager resolves the effective policy first, keeps its log lines,
	 * and writes the resolved id/type back. Unreachable legacy branches
	 * (e.g. TargetOnly + self id, already excluded from "external") are
	 * preserved in order so behavior cannot drift.
	 */
	static FSkillTargetResolution Resolve(ESkillTargetPolicy Policy, int32 TargetId, ECasterType TargetType, int32 CharacterId)
	{
		const bool bHasExternalTarget = (TargetId > 0 && TargetId != CharacterId
			&& TargetType != ECasterType::None && TargetType != ECasterType::Self);

		switch (Policy)
		{
		case ESkillTargetPolicy::SelfOnly:
			// Always target self regardless of what was passed.
			return { ETargetResolveOutcome::OkSelf, CharacterId, ECasterType::Self };

		case ESkillTargetPolicy::TargetOnly:
			// Must have a valid external target; cannot target self.
			if (!bHasExternalTarget)
			{
				return { ETargetResolveOutcome::MissingTarget, TargetId, TargetType };
			}
			if (TargetId == CharacterId)
			{
				return { ETargetResolveOutcome::SelfTargetRejected, TargetId, TargetType };
			}
			return { ETargetResolveOutcome::OkExternal, TargetId, TargetType };

		case ESkillTargetPolicy::SelfAndTarget:
			// External target wins; otherwise default to self.
			if (bHasExternalTarget)
			{
				return { ETargetResolveOutcome::OkExternal, TargetId, TargetType };
			}
			return { ETargetResolveOutcome::OkFallback, CharacterId, ECasterType::Self };

		default:
			return { ETargetResolveOutcome::UnknownPolicy, TargetId, TargetType };
		}
	}
};
