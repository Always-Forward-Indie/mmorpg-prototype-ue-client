#include "Misc/AutomationTest.h"
#include "Utils/NetCore/RemoteTurnRule.h"

// Remote turn rate: small corrections blend slowly, full reversals pivot in
// a couple of frames instead of swimming sideways for 300ms. Calls
// production FRemoteTurnRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteTurnRuleTest, "MMO.Player.RemoteTurn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRemoteTurnRuleTest::RunTest(const FString& Parameters)
{
    using R = FRemoteTurnRule;
    const float Dt = 1.0f / 60.0f;
    const float Small = R::TurnAlpha(Dt, 30.0f);
    const float Large = R::TurnAlpha(Dt, 180.0f);
    TestTrue(TEXT("alpha in range"), Small >= 0.0f && Small <= 1.0f && Large >= 0.0f && Large <= 1.0f);
    TestTrue(TEXT("reversal turns faster"), Large > Small);
    TestEqual(TEXT("zero dt holds"), R::TurnAlpha(0.0f, 180.0f), 0.0f);
    TestEqual(TEXT("huge dt clamps"), R::TurnAlpha(10.0f, 10.0f), 1.0f);
    return true;
}
