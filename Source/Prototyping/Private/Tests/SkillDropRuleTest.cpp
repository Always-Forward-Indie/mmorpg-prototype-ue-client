#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SkillDropRule.h"

// Skill drag-drop accept rule: reject the source slot itself (kills the
// highlight flicker), reject empty slugs, otherwise defer to skill
// ownership (optimistic accept without a manager — SkillBarWidget
// re-validates authoritatively on drop). Calls the production
// FSkillDropRule used by USkillSlotWidget::CanAcceptSkillDrop
// (which keeps the null check and the pointer-keyed cache).

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
    return true;
}
