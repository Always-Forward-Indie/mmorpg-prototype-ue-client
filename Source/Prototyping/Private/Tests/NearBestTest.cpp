#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SampleRule.h"

// Near-best filter: thin history accepts everything, otherwise the sample
// must sit within 20% of the best RTT over the trailing window (best
// seeds from the new sample). Calls the production FSampleRule used by
// UTimeSyncService::IsNearBestSample.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNearBestTest, "MMO.TimeSync.NearBest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNearBestTest::RunTest(const FString& Parameters)
{
    using R = FSampleRule;
    TestTrue(TEXT("thin history accepts"), R::IsNearBest({}, 1000.0f));
    TestTrue(TEXT("two samples accept"), R::IsNearBest({ 50.0f, 60.0f }, 1000.0f));
    TestTrue(TEXT("within tolerance"), R::IsNearBest({ 50.0f, 55.0f, 60.0f }, 60.0f));
    TestFalse(TEXT("too slow"), R::IsNearBest({ 50.0f, 55.0f, 60.0f }, 200.0f));
    // New sample seeds the best: a fresh record always passes.
    TestTrue(TEXT("record passes"), R::IsNearBest({ 50.0f, 55.0f, 60.0f }, 40.0f));
    // Window covers only the trailing 10: ancient slow samples don't matter.
    TArray<float> Long;
    for (int32 i = 0; i < 12; ++i) { Long.Add(500.0f); }
    Long.Add(50.0f);
    TestTrue(TEXT("windowed best"), R::IsNearBest(Long, 55.0f));
    return true;
}
