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

// Hi-res variant: RTT measured with sub-ms precision (FPlatformTime), server
// timestamps stay ms integers. Below the 2ms quantum floor the integer server
// processing cannot resolve a ratio, so only the ratio gate is relaxed there;
// every other bound is identical to the ms variant. Calls production
// FSampleRule::IsValidSampleHiRes directly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSampleRuleHiResTest, "MMO.TimeSync.SampleRuleHiRes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSampleRuleHiResTest::RunTest(const FString& Parameters)
{
    using R = FSampleRule;
    // Normal sample passes.
    TestTrue(TEXT("good sample"), R::IsValidSampleHiRes(30.0, 1010, 1015));
    // True-zero RTT is a clock anomaly, not a fast network.
    TestFalse(TEXT("zero rtt"), R::IsValidSampleHiRes(0.0, 1000, 1000));
    // Sub-ms local sample with 1ms-quantized processing passes (the old
    // ms-quantum rule rejected these, starving localhost sync history).
    TestTrue(TEXT("lan quantum sample"), R::IsValidSampleHiRes(1.2, 1000, 1001));
    TestTrue(TEXT("lan zero processing"), R::IsValidSampleHiRes(0.6, 1000, 1000));
    // WAN ratio gate still applies where measurable (RTT >= 2ms).
    TestFalse(TEXT("ratio over cap"), R::IsValidSampleHiRes(100.0, 0, 85));
    TestTrue(TEXT("ratio boundary"), R::IsValidSampleHiRes(100.0, 0, 80));
    // Processing beyond RTT + 1ms quantum slop is still a clock issue.
    TestFalse(TEXT("negative latency"), R::IsValidSampleHiRes(50.0, 0, 60));
    // Bounds still enforced.
    TestFalse(TEXT("rtt over cap"), R::IsValidSampleHiRes(1500.0, 10, 15));
    TestFalse(TEXT("processing over cap"), R::IsValidSampleHiRes(200.0, 0, 150));
    TestFalse(TEXT("negative processing"), R::IsValidSampleHiRes(100.0, 10, 5));
    return true;
}
