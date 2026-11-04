#include "Misc/AutomationTest.h"
#include "Utils/NetCore/TargetingRule.h"

// Crosshair soft-target acceptance: range + cone, head-plate culling beyond range.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingRuleTest, "MMO.Targeting.RangeRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTargetingRuleTest::RunTest(const FString& Parameters)
{
    using R = FTargetingRule;
    TestTrue(TEXT("in range"), R::IsInRange(1400.f, 1500.f));
    TestFalse(TEXT("out of range"), R::IsInRange(1600.f, 1500.f));
    TestTrue(TEXT("in cone"), R::IsInCone(0.98f, 0.97f));
    TestFalse(TEXT("out of cone"), R::IsInCone(0.90f, 0.97f));
    TestTrue(TEXT("accept"), R::AcceptSoftTarget(1400.f, 1500.f, 0.98f));
    TestFalse(TEXT("reject range"), R::AcceptSoftTarget(1600.f, 1500.f, 0.99f));
    TestFalse(TEXT("reject cone"), R::AcceptSoftTarget(1400.f, 1500.f, 0.90f));
    TestTrue(TEXT("cull"), R::ShouldCullHeadPlate(2600.f, 2500.f));
    TestFalse(TEXT("no cull"), R::ShouldCullHeadPlate(2400.f, 2500.f));
    // Zoom-independent trace: gameplay range from the player + camera distance.
    TestEqual(TEXT("far zoom keeps range"), R::EffectiveTraceLength(1200.f, 1500.f), 2700.f);
    TestEqual(TEXT("close zoom keeps range"), R::EffectiveTraceLength(200.f, 1500.f), 1700.f);
    TestEqual(TEXT("zero cam dist"), R::EffectiveTraceLength(0.f, 1500.f), 1500.f);
    TestEqual(TEXT("clamped negative"), R::EffectiveTraceLength(-50.f, 1500.f), 1500.f);
    return true;
}
