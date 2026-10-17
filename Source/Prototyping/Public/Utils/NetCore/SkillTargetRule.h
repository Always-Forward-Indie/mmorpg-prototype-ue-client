#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

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
};
