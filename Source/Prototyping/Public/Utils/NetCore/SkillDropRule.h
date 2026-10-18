#pragma once

#include "CoreMinimal.h"

/**
 * Skill drag-drop accept rule (pure logic, no widgets).
 *
 * Canonical owner of the drop predicate used by
 * USkillSlotWidget::CanAcceptSkillDrop (which keeps the null check and the
 * cached inputs - both widget concerns). Reject the source slot itself
 * (kills the highlight flicker), reject empty slugs, otherwise defer to skill
 * ownership (optimistic accept when no manager is bound - SkillBarWidget
 * re-validates authoritatively on drop). Pinned by MMO.Skills.DropRule.
 *
 * The cache key is the op pointer PLUS the ownership inputs: the cached
 * answer goes stale when skill ownership changes mid-drag (learning a skill
 * re-fires OnSkillsInitialized). ShouldInvalidateCache owns that decision;
 * the widget only stores the inputs.
 */
struct PROTOTYPING_API FSkillDropRule
{
	static bool CanAccept(bool bIsSameSource, bool bSlugEmpty, bool bHasManager, bool bHasSkill)
	{
		if (bIsSameSource) { return false; }
		if (bSlugEmpty) { return false; }
		return bHasManager ? bHasSkill : true;
	}

	/** True when the cached answer must be recomputed (new op, manager appeared/vanished, ownership flipped). */
	static bool ShouldInvalidateCache(bool bSameOp, bool bCachedHadManager, bool bHadManager, bool bCachedHasSkill, bool bHasSkillNow)
	{
		if (!bSameOp) { return true; }
		if (bCachedHadManager != bHadManager) { return true; }
		return bCachedHasSkill != bHasSkillNow;
	}
};
