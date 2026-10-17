#include "Misc/AutomationTest.h"
#include "Utils/NetCore/RetryPolicy.h"

// Connection-retry contract for the Login/Game/Chunk sockets: 1.0s poll
// ticks, waiting continues while RetryCount <= MaxRetries, the run after
// that fails. Calls the production FRetryPolicy used by UNetworkManager.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRetryPolicyTest, "MMO.Network.RetryPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRetryPolicyTest::RunTest(const FString& Parameters)
{
    using P = FRetryPolicy;
    TestEqual(TEXT("tick interval pinned"), P::ConnectionRetryIntervalSec, 1.0f);
    // Shipped budget: 3 retries per socket (MaxLogin/Game/ChunkRetries).
    TestTrue(TEXT("first retry waits"), P::ShouldKeepWaiting(1, 3));
    TestTrue(TEXT("last retry waits"), P::ShouldKeepWaiting(3, 3));
    TestFalse(TEXT("past budget fails"), P::ShouldKeepWaiting(4, 3));
    TestFalse(TEXT("not exceeded at budget"), P::HasExceededMaxRetries(3, 3));
    TestTrue(TEXT("exceeded past budget"), P::HasExceededMaxRetries(4, 3));
    // Predicates are exact inverses for any budget.
    for (int32 Count = 0; Count <= 5; ++Count)
    {
        TestEqual(TEXT("inverse predicates"),
            P::ShouldKeepWaiting(Count, 3), !P::HasExceededMaxRetries(Count, 3));
    }
    // Zero budget: fail immediately on first failed tick.
    TestFalse(TEXT("zero budget waits never"), P::ShouldKeepWaiting(1, 0));
    TestTrue(TEXT("zero budget exceeded at once"), P::HasExceededMaxRetries(1, 0));
    return true;
}
