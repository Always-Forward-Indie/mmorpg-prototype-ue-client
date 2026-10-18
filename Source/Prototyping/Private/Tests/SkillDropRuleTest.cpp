#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SkillDropRule.h"

// Skill drag-drop accept rule: reject the source slot itself (kills the
// highlight flicker), reject empty slugs, otherwise defer to skill
// ownership (optimistic accept without a manager — SkillBarWidget
// re-validates authoritatively on drop). The cache key is the op pointer
// PLUS the ownership inputs (ShouldInvalidateCache), so learning a skill
// mid-drag recomputes instead of serving stale. Calls the production
// FSkillDropRule used by USkillSlotWidget::CanAcceptSkillDrop
// (which keeps the null check and stores the cached inputs).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillDropRuleTest, "MMO.Skills.DropRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillDropRuleTest::RunTest(const FString& Parameters)
{
    using R = FSkillDropRule;
    TestFalse(TEXT("same source rejected"), R::CanAccept(true, false, true, true));
    TestFalse(TEXT("empty slug rejected"), R::CanAccept(false, true, true, true));
    TestFalse(TEXT("unowned rejected"), R::CanAccept(false, false, true, false));
    TestTrue(TEXT("owned accepted"), R::CanAccept(false, false, true, true));
    TestTrue(TEXT("no manager optimistic"), R::CanAccept(false, false, false, false));

    // Cache key is op pointer PLUS ownership inputs: recompute when the op
    // is new, the manager appeared/vanished, or ownership flipped mid-drag.
    TestFalse(TEXT("same op same inputs keeps cache"),
        R::ShouldInvalidateCache(true, true, true, true, true));
    TestFalse(TEXT("same op still-unknown keeps cache"),
        R::ShouldInvalidateCache(true, true, true, false, false));
    TestTrue(TEXT("new op invalidates"), R::ShouldInvalidateCache(false, true, true, true, true));
    TestTrue(TEXT("manager appeared invalidates"), R::ShouldInvalidateCache(true, false, true, false, false));
    TestTrue(TEXT("manager vanished invalidates"), R::ShouldInvalidateCache(true, true, false, true, true));
    TestTrue(TEXT("skill learned invalidates"), R::ShouldInvalidateCache(true, true, true, false, true));
    TestTrue(TEXT("skill lost invalidates"), R::ShouldInvalidateCache(true, true, true, true, false));
    return true;
}
