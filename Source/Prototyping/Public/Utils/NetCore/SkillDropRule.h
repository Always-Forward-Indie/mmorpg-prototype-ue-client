#pragma once

#include "CoreMinimal.h"

/**
 * Skill drag-drop accept rule (pure logic, no widgets).
 *
 * Canonical owner of the drop predicate used by
 * USkillSlotWidget::CanAcceptSkillDrop (which keeps the null check and
 * the pointer-keyed result cache — both widget concerns). Reject the
 * source slot itself (kills the highlight flicker), reject empty slugs,
 * otherwise defer to skill ownership (optimistic accept when no manager
 * is bound — SkillBarWidget re-validates authoritatively on drop).
 * Pinned by MMO.Skills.DropRule.
 */
struct PROTOTYPING_API FSkillDropRule
{
	static bool CanAccept(bool bIsSameSource, bool bSlugEmpty, bool bHasManager, bool bHasSkill)
	{
		if (bIsSameSource) { return false; }
		if (bSlugEmpty) { return false; }
		return bHasManager ? bHasSkill : true;
	}
};
