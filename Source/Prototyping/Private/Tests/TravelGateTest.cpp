#include "Misc/AutomationTest.h"
#include "Utils/NetCore/TravelGate.h"

// Travel/readiness timeout policy (S08): soft safety fires at the timeout,
// hard watchdog at 8s, frame countdown gives up at 3s, travel crawl needs
// both slowness (>5s) and starvation (<5fps). Calls production FTravelGate.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTravelGateTest, "MMO.Travel.Gate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTravelGateTest::RunTest(const FString& Parameters)
{
    using G = FTravelGate;
    TestFalse(TEXT("soft not yet"), G::ShouldForceHide(14.9, 15.0));
    TestTrue(TEXT("soft at timeout"), G::ShouldForceHide(15.0, 15.0));
    TestTrue(TEXT("soft past timeout"), G::ShouldForceHide(30.0, 15.0));
    TestEqual(TEXT("hard watchdog"), G::HardWatchdogSec, 8.0);
    TestFalse(TEXT("hard not yet"), G::IsHardWatchdogExpired(7.9));
    TestTrue(TEXT("hard at 8s"), G::IsHardWatchdogExpired(8.0));
    TestEqual(TEXT("frame timeout"), G::FrameCountdownTimeoutSec, 3.0);
    TestFalse(TEXT("frame waits"), G::ShouldGiveUpFrameCountdown(2.9));
    TestTrue(TEXT("frame gives up"), G::ShouldGiveUpFrameCountdown(3.0));
    TestFalse(TEXT("fast travel fine"), G::HasTravelStalled(2.0, 10));
    TestFalse(TEXT("slow but rendering"), G::HasTravelStalled(10.0, 300));
    TestTrue(TEXT("crawl detected"), G::HasTravelStalled(10.0, 10));
    TestFalse(TEXT("zero time"), G::HasTravelStalled(0.0, 0));
    return true;
}
