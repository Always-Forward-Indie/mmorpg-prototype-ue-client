#pragma once

#include "CoreMinimal.h"

/**
 * Skill-bar slot index rule (pure logic, no UObject).
 *
 * Canonical owner of the valid-slot predicate used by
 * UPlayerSkillManager::ValidateSkillSlot (default MaxSkillSlots = 10).
 * Pinned by MMO.Skills.SlotRule.
 */
struct PROTOTYPING_API FSkillSlotRule
{
	static constexpr int32 DefaultMaxSlots = 10;

	static bool IsValidSlot(int32 SlotIndex, int32 MaxSlots) { return SlotIndex >= 0 && SlotIndex < MaxSlots; }
};
