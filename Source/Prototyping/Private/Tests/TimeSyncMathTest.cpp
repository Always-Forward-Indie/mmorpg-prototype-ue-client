#include "Misc/AutomationTest.h"
#include "Utils/NetCore/TimeSyncMath.h"

// NTP math for UTimeSyncService time sync (t0 sent, t1 server-recv,
// t2 server-send, t3 received). Calls the production FTimeSyncMath —
// no local re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimeSyncMathTest, "MMO.TimeSync.NtpMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTimeSyncMathTest::RunTest(const FString& Parameters)
{
    // t0=1000, t1=1010, t2=1015, t3=1030 -> offset -2, latency 12.5.
    TestEqual(TEXT("ntp offset"), FTimeSyncMath::OffsetMs(1000, 1010, 1015, 1030), (int64)-2);
    TestEqual(TEXT("ntp latency"), FTimeSyncMath::LatencyMs(1000, 1010, 1015, 1030), 12.5f);
    // Symmetric zero-delay loopback.
    TestEqual(TEXT("loopback offset"), FTimeSyncMath::OffsetMs(500, 500, 500, 500), (int64)0);
    TestEqual(TEXT("loopback latency"), FTimeSyncMath::LatencyMs(500, 500, 500, 500), 0.0f);
    // Half-millisecond rounding: (4 + 3) / 2 = 3.5 rounds to 4.
    // Integer truncation would give 3 — this case pins the production
    // double-precision + RoundToDouble semantics against regression.
    TestEqual(TEXT("half rounds up"), FTimeSyncMath::OffsetMs(1000, 1004, 1005, 1002), (int64)4);
    TestEqual(TEXT("half latency"), FTimeSyncMath::LatencyMs(1000, 1004, 1005, 1002), 0.5f);
    return true;
}
