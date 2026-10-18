#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ChunkLinkRule.h"

// Mid-session chunk-link predicate (P0.2 reconnect): the poll loop starts a
// bounded reconnect only for an established session with a down link and no
// reconnect already running. Calls the production FChunkLinkRule used by
// UNetworkManager::PollChunkServerNetworkData — no re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChunkLinkRuleTest, "MMO.Network.ChunkLinkRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FChunkLinkRuleTest::RunTest(const FString& Parameters)
{
    using R = FChunkLinkRule;
    TestTrue(TEXT("connected link up"), !R::IsLinkDown(true));
    TestTrue(TEXT("dropped link down"), R::IsLinkDown(false));

    // The only case that starts a reconnect: live session, dead link, idle.
    TestTrue(TEXT("established+down starts"), R::ShouldStartReconnect(true, true, false));
    // Never reconnect while connecting (no session yet — timer owns that).
    TestFalse(TEXT("no session never starts"), R::ShouldStartReconnect(false, true, false));
    TestFalse(TEXT("no session up never starts"), R::ShouldStartReconnect(false, false, false));
    // Healthy link never reconnects, even mid-session.
    TestFalse(TEXT("healthy link never starts"), R::ShouldStartReconnect(true, false, false));
    // A running reconnect must not spawn a second one (timer guards it).
    TestFalse(TEXT("in-flight never restarts"), R::ShouldStartReconnect(true, true, true));
    TestFalse(TEXT("in-flight healthy never restarts"), R::ShouldStartReconnect(true, false, true));
    return true;
}
