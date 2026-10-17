#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SampleRule.h"

// NTP sample validity gate (t0 sent, t1 server-recv, t2 server-send,
// t3 received): RTT bounds, server-processing bounds, non-negative NTP
// latency, processing/RTT ratio cap. Calls the production FSampleRule
// used by UTimeSyncService::IsSampleValid. Bounds mirror the service
// tunables (RTT 1000ms, processing 100ms, ratio 0.8).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSampleRuleTest, "MMO.TimeSync.SampleRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSampleRuleTest::RunTest(const FString& Parameters)
{
    using R = FSampleRule;
    TestTrue(TEXT("good sample"), R::IsValidSample(1000, 1010, 1015, 1030));
    TestFalse(TEXT("zero rtt"), R::IsValidSample(1000, 1000, 1000, 1000));
    TestFalse(TEXT("rtt over cap"), R::IsValidSample(0, 10, 15, 1500));
    TestFalse(TEXT("negative processing"), R::IsValidSample(0, 10, 5, 100));
    TestFalse(TEXT("processing over cap"), R::IsValidSample(0, 0, 150, 200));
    // RT < SP with sane parts: latency goes negative (clock issue).
    TestFalse(TEXT("negative latency"), R::IsValidSample(0, 0, 60, 50));
    // Processing dominates the round trip (overloaded server).
    TestFalse(TEXT("ratio over cap"), R::IsValidSample(0, 0, 85, 100));
    // Exact ratio boundary still passes (strictly-greater check).
    TestTrue(TEXT("ratio boundary"), R::IsValidSample(0, 0, 80, 100));
    // Custom bounds are honored.
    TestFalse(TEXT("custom rtt cap"), R::IsValidSample(0, 10, 15, 200, 100.0f));
    return true;
}
