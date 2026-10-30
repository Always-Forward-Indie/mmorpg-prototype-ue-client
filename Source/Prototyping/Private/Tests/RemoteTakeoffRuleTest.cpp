#include "Misc/AutomationTest.h"
#include "Utils/NetCore/RemoteTakeoffRule.h"

// Remote takeoff: fast rendered vertical motion means airborne even when
// the server flag still reads ground (flag arrives only inside movement
// packets). Calls production FRemoteTakeoffRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteTakeoffRuleTest, "MMO.Player.RemoteTakeoff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRemoteTakeoffRuleTest::RunTest(const FString& Parameters)
{
    using R = FRemoteTakeoffRule;
    TestTrue(TEXT("threshold positive"), R::AirborneVzThreshold() > 0.0f);
    // Jump start: flag lags, Vz large -> airborne.
    TestTrue(TEXT("rising takes off"), R::ShouldTakeOff(false, 450.0f));
    TestTrue(TEXT("falling takes off"), R::ShouldTakeOff(false, -450.0f));
    // Stairs: flag ground, Vz moderate -> stays grounded.
    TestFalse(TEXT("stair climb stays grounded"), R::ShouldTakeOff(false, 100.0f));
    TestFalse(TEXT("standing stays grounded"), R::ShouldTakeOff(false, 0.0f));
    // Flag already airborne -> rule defers to it (no double handling).
    TestFalse(TEXT("flag air needs no takeoff"), R::ShouldTakeOff(true, 450.0f));
    return true;
}
