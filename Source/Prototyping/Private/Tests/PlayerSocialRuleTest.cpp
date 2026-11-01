#include "Misc/AutomationTest.h"
#include "Utils/NetCore/PlayerSocialRule.h"

// Player unit-menu gating + trade-range mirror. Calls production code.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerSocialRuleTest, "MMO.Social.Rule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPlayerSocialRuleTest::RunTest(const FString& Parameters)
{
    using R = FPlayerSocialRule;

    // Menu opens for a live non-self remote player only.
    TestTrue(TEXT("live target opens"), R::CanOpenMenu(false, true, false, false));
    TestFalse(TEXT("dead local blocks"), R::CanOpenMenu(true, true, false, false));
    TestFalse(TEXT("null target blocks"), R::CanOpenMenu(false, false, false, false));
    TestFalse(TEXT("self blocks"), R::CanOpenMenu(false, true, true, false));
    TestFalse(TEXT("dead target blocks"), R::CanOpenMenu(false, true, false, true));

    // Trade range: inside passes, outside fails (server re-validates).
    TestTrue(TEXT("point blank trades"), R::CanRequestTrade(0.f));
    TestTrue(TEXT("edge trades"), R::CanRequestTrade(R::TradeRangeCm));
    TestFalse(TEXT("beyond range blocked"), R::CanRequestTrade(R::TradeRangeCm + 1.f));

    // Click-vs-drag thresholds are sane (positive, sub-second, few pixels).
    TestTrue(TEXT("click window positive"), R::MenuClickMaxSeconds > 0.f);
    TestTrue(TEXT("click window sub-second"), R::MenuClickMaxSeconds < 1.f);
    TestTrue(TEXT("drag threshold positive"), R::MenuClickMaxPixels > 0.f);
    return true;
}
