#include "Misc/AutomationTest.h"
#include "Utils/NetCore/RemoteTakeoffRule.h"

// Corroborated takeoff: vertical speed alone flaps in the landing tail
// (flag reads ground while interpolation still descends), so takeoffs need
// server-target corroboration, asymmetric by direction. Calls production
// FRemoteTakeoffRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteTakeoffRuleTest, "MMO.Player.RemoteTakeoff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRemoteTakeoffRuleTest::RunTest(const FString& Parameters)
{
    using R = FRemoteTakeoffRule;
    TestTrue(TEXT("threshold positive"), R::AirborneVzThreshold() > 0.0f);
    // Jump start: rising fast, server target leading above.
    TestTrue(TEXT("rising takes off"), R::ShouldTakeOffRising(450.0f, 20.0f));
    // Snap artifact: huge Vz but target already reached -> ignore.
    TestFalse(TEXT("snap ignored"), R::ShouldTakeOffRising(1161.0f, 0.0f));
    TestFalse(TEXT("snap ignored falling"), R::ShouldTakeOffFalling(-1161.0f, 0.0f, 5.0f));
    // Stairs: moderate Vz never takes off.
    TestFalse(TEXT("stair climb grounded"), R::ShouldTakeOffRising(100.0f, 50.0f));
    // Landing tail: descending toward a below-target fresh off air -> suppress.
    TestFalse(TEXT("landing tail suppressed"), R::ShouldTakeOffFalling(-246.0f, -30.0f, 0.05f));
    // Cliff edge: descending toward below-target long after air -> take off.
    TestTrue(TEXT("cliff fall takes off"), R::ShouldTakeOffFalling(-300.0f, -40.0f, 2.0f));
    return true;
}
