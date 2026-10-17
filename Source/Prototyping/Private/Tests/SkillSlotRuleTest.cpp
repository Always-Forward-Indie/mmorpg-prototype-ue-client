#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SkillSlotRule.h"

// Skill-bar slot bounds: 0 <= index < MaxSlots (default 10). Calls the
// production FSkillSlotRule used by UPlayerSkillManager::ValidateSkillSlot.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillSlotRuleTest, "MMO.Skills.SlotRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillSlotRuleTest::RunTest(const FString& Parameters)
{
    using R = FSkillSlotRule;
    TestEqual(TEXT("default max"), R::DefaultMaxSlots, 10);
    TestFalse(TEXT("negative"), R::IsValidSlot(-1, 10));
    TestTrue(TEXT("first"), R::IsValidSlot(0, 10));
    TestTrue(TEXT("last"), R::IsValidSlot(9, 10));
    TestFalse(TEXT("past end"), R::IsValidSlot(10, 10));
    TestFalse(TEXT("zero budget"), R::IsValidSlot(0, 0));
    TestTrue(TEXT("custom budget"), R::IsValidSlot(4, 5));
    return true;
}
