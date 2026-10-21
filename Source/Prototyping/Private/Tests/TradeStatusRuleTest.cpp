#include "Misc/AutomationTest.h"
#include "Utils/NetCore/TradeStatusRule.h"

// P2P trade session status (B3): a session is the non-empty server session
// id; confirm is allowed while it lives and we have not confirmed;
// only cancelled/completed wipe the snapshot. Calls the production
// FTradeStatusRule used by UTradeManager/UTradeWidget — no re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeStatusRuleTest, "MMO.Trade.StatusRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTradeStatusRuleTest::RunTest(const FString& Parameters)
{
    using R = FTradeStatusRule;
    TestFalse(TEXT("empty id is no session"), R::HasActiveSession(TEXT("")));
    TestTrue(TEXT("non-empty id is session"), R::HasActiveSession(TEXT("sess-1")));

    TestTrue(TEXT("confirm when live + pending"), R::CanConfirm(TEXT("sess-1"), false));
    TestFalse(TEXT("no confirm after own confirm"), R::CanConfirm(TEXT("sess-1"), true));
    TestFalse(TEXT("no confirm without session"), R::CanConfirm(TEXT(""), false));

    TestTrue(TEXT("cancelled clears"), R::ShouldClearSession(TEXT("tradeCancelled")));
    TestTrue(TEXT("complete clears"), R::ShouldClearSession(TEXT("tradeComplete")));
    TestTrue(TEXT("clear match is case-insensitive"), R::ShouldClearSession(TEXT("TradeCancelled")));
    TestFalse(TEXT("declined preserves"), R::ShouldClearSession(TEXT("tradeDeclined")));
    TestFalse(TEXT("state preserves"), R::ShouldClearSession(TEXT("tradeState")));
    TestFalse(TEXT("invite preserves"), R::ShouldClearSession(TEXT("tradeInvite")));
    TestFalse(TEXT("empty preserves"), R::ShouldClearSession(TEXT("")));
    TestFalse(TEXT("unknown preserves"), R::ShouldClearSession(TEXT("trade frobnicate")));

    // Initiator election: lower character id invites, higher accepts.
    TestTrue(TEXT("lower id initiates"), R::ShouldInitiateTrade(614, 615));
    TestFalse(TEXT("higher id accepts"), R::ShouldInitiateTrade(615, 614));
    TestFalse(TEXT("same id never initiates"), R::ShouldInitiateTrade(614, 614));
    TestFalse(TEXT("unknown own never initiates"), R::ShouldInitiateTrade(0, 615));
    TestFalse(TEXT("unknown peer never initiates"), R::ShouldInitiateTrade(614, 0));

    // Accept/decline echo: the server matches the pending invite on the
    // INITIATOR's id — echoing our own id answers "no_pending_invite".
    TestEqual(TEXT("accept echoes initiator"), R::AcceptEchoField(614), TEXT("614"));
    TestEqual(TEXT("accept echo empty without invite"), R::AcceptEchoField(0), TEXT(""));
    TestEqual(TEXT("accept echo empty for negative"), R::AcceptEchoField(-3), TEXT(""));
    return true;
}
