#include "Misc/AutomationTest.h"
#include "Utils/NetCore/RemoteIdleResetRule.h"

// Remote idle reset: any displacement (planar OR vertical) restarts the
// idle clock. XY-only reset lets in-place jumps run the clock mid-flight
// and flap the fall flag. Calls production FRemoteIdleResetRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteIdleResetRuleTest, "MMO.Player.RemoteIdleReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRemoteIdleResetRuleTest::RunTest(const FString& Parameters)
{
    using R = FRemoteIdleResetRule;
    TestTrue(TEXT("planar step resets"), R::ShouldResetIdle(5.0f, 0.0f));
    TestTrue(TEXT("vertical jump resets"), R::ShouldResetIdle(0.0f, 40.0f));
    TestTrue(TEXT("vertical drop resets"), R::ShouldResetIdle(0.0f, -40.0f));
    TestFalse(TEXT("stillness never resets"), R::ShouldResetIdle(0.0f, 0.0f));
    TestFalse(TEXT("sub-cm jitter never resets"), R::ShouldResetIdle(0.5f, 0.5f));
    return true;
}
