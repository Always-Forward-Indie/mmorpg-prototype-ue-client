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
    // Tab cycle: front hemisphere only, reset-vs-cycle from the visible set.
    TestTrue(TEXT("front"), R::IsInFront(0.5f));
    TestFalse(TEXT("side plane"), R::IsInFront(0.0f));
    TestFalse(TEXT("behind"), R::IsInFront(-0.3f));
    TestTrue(TEXT("tab accept"), R::AcceptTabTarget(0.9f, 0.0f, true));
    TestFalse(TEXT("tab reject behind"), R::AcceptTabTarget(-0.5f, 0.0f, true));
    TestFalse(TEXT("tab reject blocked"), R::AcceptTabTarget(0.9f, 0.0f, false));
    TestTrue(TEXT("tab order angle"), R::TabCandidateLess(0.99f, 1e6f, 0.90f, 1.0f));
    TestFalse(TEXT("tab order angle reverse"), R::TabCandidateLess(0.90f, 1.0f, 0.99f, 1e6f));
    TestTrue(TEXT("tab order dist tiebreak"), R::TabCandidateLess(0.99f, 50.f, 0.995f, 100.f));
    TestFalse(TEXT("tab order dist tiebreak reverse"), R::TabCandidateLess(0.995f, 100.f, 0.99f, 50.f));
    TestEqual(TEXT("tab reset when lock invisible"), R::TabStartIndex(INDEX_NONE, 3), 0);
    TestEqual(TEXT("tab reset when lock stale"), R::TabStartIndex(7, 3), 0);
    TestEqual(TEXT("tab advance"), R::TabStartIndex(1, 3), 2);
    TestEqual(TEXT("tab wrap"), R::TabStartIndex(2, 3), 0);
    TestEqual(TEXT("tab empty"), R::TabStartIndex(INDEX_NONE, 0), INDEX_NONE);
    return true;
}
