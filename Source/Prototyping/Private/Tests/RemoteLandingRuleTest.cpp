#include "Misc/AutomationTest.h"
#include "Utils/NetCore/RemoteLandingRule.h"

// Remote landing: airborne + no movement for a full grace window = landed.
// A still-standing remote sends no packets, so without this the fall state
// sticks until the next move/rotate packet. Calls production FRemoteLandingRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteLandingRuleTest, "MMO.Player.RemoteLanding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRemoteLandingRuleTest::RunTest(const FString& Parameters)
{
    using R = FRemoteLandingRule;
    TestTrue(TEXT("idle past grace lands"), R::ShouldLand(true, false, 0.2f, 0.15f));
    TestTrue(TEXT("idle at grace lands"), R::ShouldLand(true, false, 0.15f, 0.15f));
    TestFalse(TEXT("idle inside grace stays"), R::ShouldLand(true, false, 0.1f, 0.15f));
    TestFalse(TEXT("moving stays airborne"), R::ShouldLand(true, true, 5.0f, 0.15f));
    TestFalse(TEXT("grounded never lands"), R::ShouldLand(false, false, 5.0f, 0.15f));
    return true;
}
