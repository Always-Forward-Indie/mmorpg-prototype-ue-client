#include "Misc/AutomationTest.h"
#include "Utils/NetCore/CooldownFormat.h"

// Travel re-anchor rule: non-positive remainings are dropped (never
// resurrect), live remainings re-anchor to now+remaining, over-long
// remainings clamp to the entry maximum. Calls production FCooldownAnchor.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCooldownAnchorTest, "MMO.Skills.CooldownAnchor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCooldownAnchorTest::RunTest(const FString& Parameters)
{
    using A = FCooldownAnchor;
    double End = -1.0;

    TestFalse(TEXT("zero remaining dropped"), A::Reanchor(100.0, 0.0, 10.0, End));
    TestEqual(TEXT("end untouched on drop"), End, -1.0);
    TestFalse(TEXT("negative remaining dropped"), A::Reanchor(100.0, -5.0, 10.0, End));

    TestTrue(TEXT("live remaining carried"), A::Reanchor(100.0, 4.0, 10.0, End));
    TestEqual(TEXT("end re-anchored"), End, 104.0);

    TestTrue(TEXT("over-long clamped"), A::Reanchor(50.0, 5000.0, 10.0, End));
    TestEqual(TEXT("end clamped to max"), End, 60.0);

    TestEqual(TEXT("gcd cap"), A::MaxGcdCarrySec, 10.0);
    return true;
}
