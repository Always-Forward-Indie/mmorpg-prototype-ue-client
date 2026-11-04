#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ItemCooldownRule.h"

// Predictive item cooldown overlay math: remaining clamps at zero,
// ratio is clamped 0..1, format delegates to the shared cooldown formatter.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemCooldownRuleTest, "MMO.Items.CooldownRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemCooldownRuleTest::RunTest(const FString& Parameters)
{
    using R = FItemCooldownRule;
    TestEqual(TEXT("remaining active"), R::Remaining(10.0f, 15.0f), 5.0f);
    TestEqual(TEXT("remaining expired"), R::Remaining(20.0f, 15.0f), 0.0f);
    TestTrue(TEXT("is active"), R::IsActive(10.0f, 15.0f));
    TestFalse(TEXT("equality ready"), R::IsActive(15.0f, 15.0f));
    TestEqual(TEXT("ratio half"), R::Ratio(5.0f, 10.0f), 0.5f);
    TestEqual(TEXT("ratio clamped"), R::Ratio(15.0f, 10.0f), 1.0f);
    TestEqual(TEXT("ratio zero total"), R::Ratio(5.0f, 0.0f), 0.0f);
    return true;
}
