#include "Misc/AutomationTest.h"

// NTP math mirror of UTimeSyncService::CalculateNTPOffset/CalculateNTPLatency.
// t0 sent, t1 server-recv, t2 server-send, t3 received.
namespace
{
int64 NtpOffset(int64 T0, int64 T1, int64 T2, int64 T3) { return ((T1 - T0) + (T2 - T3)) / 2; }
float NtpLatency(int64 T0, int64 T1, int64 T2, int64 T3) { return ((T3 - T0) - (T2 - T1)) / 2.0f; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTimeSyncMathTest, "MMO.TimeSync.NtpMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTimeSyncMathTest::RunTest(const FString& Parameters)
{
    // t0=1000, t1=1010, t2=1015, t3=1030 -> offset -2 (int div of -5/2), latency 12.5.
    TestEqual(TEXT("ntp offset"), NtpOffset(1000, 1010, 1015, 1030), (int64)-2);
    TestEqual(TEXT("ntp latency"), NtpLatency(1000, 1010, 1015, 1030), 12.5f);
    // Symmetric zero-delay loopback.
    TestEqual(TEXT("loopback offset"), NtpOffset(500, 500, 500, 500), (int64)0);
    TestEqual(TEXT("loopback latency"), NtpLatency(500, 500, 500, 500), 0.0f);
    return true;
}
