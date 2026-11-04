#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SkillFeedbackRule.h"

// Skill UI feedback gating: deny throttling, start/ready audibility thresholds.
// Calls the production FSkillFeedbackRule used by USkillBarWidget.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillFeedbackRuleTest, "MMO.Skills.FeedbackRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillFeedbackRuleTest::RunTest(const FString& Parameters)
{
    using R = FSkillFeedbackRule;
    // Deny throttle: first play allowed, immediate repeat blocked, later allowed.
    TestTrue(TEXT("deny first"), R::CanPlayDeny(10.0f, -10000.0f));
    TestFalse(TEXT("deny throttled"), R::CanPlayDeny(10.1f, 10.0f));
    TestTrue(TEXT("deny after interval"), R::CanPlayDeny(10.5f, 10.0f));
    // Start: assigned + long enough + not restore.
    TestTrue(TEXT("start audible"), R::ShouldPlayStart(5.0f, true, false));
    TestFalse(TEXT("start unassigned"), R::ShouldPlayStart(5.0f, false, false));
    TestFalse(TEXT("start restore"), R::ShouldPlayStart(5.0f, true, true));
    TestFalse(TEXT("start short"), R::ShouldPlayStart(1.0f, true, false));
    // Ready: assigned + long enough.
    TestTrue(TEXT("ready audible"), R::ShouldPlayReady(5.0f, true));
    TestFalse(TEXT("ready unassigned"), R::ShouldPlayReady(5.0f, false));
    TestFalse(TEXT("ready short"), R::ShouldPlayReady(1.0f, true));
    return true;
}
