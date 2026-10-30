#include "Misc/AutomationTest.h"
#include "Utils/NetCore/RemoteLandingRule.h"

// Remote landing: displacement-idle races the ~200ms broadcast rate and
// fires between packets mid-flight. Land only when the body is vertically
// still, converged onto the server target, and packets went quiet.
// Calls production FRemoteLandingRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteLandingRuleTest, "MMO.Player.RemoteLanding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRemoteLandingRuleTest::RunTest(const FString& Parameters)
{
    using R = FRemoteLandingRule;
    // Landed and silent: all three signals agree.
    TestTrue(TEXT("quiet settled lands"), R::ShouldLand(true, false, 0.0f, 200.0f, 0.0f, 0.5f));
    // Mid-flight packet gap (the observed bug): packets fresh -> trust flag.
    TestFalse(TEXT("fresh packets never infer"), R::ShouldLand(true, false, 0.0f, 200.0f, 0.0f, 0.2f));
    // Descending toward a below-target: still flying.
    TestFalse(TEXT("vertical motion blocks"), R::ShouldLand(true, false, 246.0f, 200.0f, 30.0f, 0.5f));
    // Not converged onto the target: interpolating, trust the flag.
    TestFalse(TEXT("unsettled blocks"), R::ShouldLand(true, false, 0.0f, 200.0f, 50.0f, 0.5f));
    // Moving horizontally: trust the flag.
    TestFalse(TEXT("moving blocks"), R::ShouldLand(true, true, 0.0f, 200.0f, 0.0f, 5.0f));
    // Flag already ground: nothing to do.
    TestFalse(TEXT("grounded never lands"), R::ShouldLand(false, false, 0.0f, 200.0f, 0.0f, 5.0f));
    return true;
}
