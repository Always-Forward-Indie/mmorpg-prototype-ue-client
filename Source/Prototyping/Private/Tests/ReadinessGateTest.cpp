#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ReadinessGate.h"

// Loading-screen readiness gate: hidden only when ALL required flags are
// set (PlayerReadyAck | UIInitialized | PlayerSpawned — StatsReceived
// excluded). Calls the production FReadinessGate used by
// UMyGameInstance::CheckAllReadyFlags — no local re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReadinessGateTest, "MMO.Readiness.Gate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FReadinessGateTest::RunTest(const FString& Parameters)
{
    using G = FReadinessGate;
    TestFalse(TEXT("nothing set"), G::AreAllRequiredSet(0));
    TestFalse(TEXT("ack only"), G::AreAllRequiredSet(G::PlayerReadyAck));
    TestFalse(TEXT("ui only"), G::AreAllRequiredSet(G::UIInitialized));
    TestFalse(TEXT("spawned only"), G::AreAllRequiredSet(G::PlayerSpawned));
    TestFalse(TEXT("stats only never suffices"),
        G::AreAllRequiredSet(G::StatsReceived));
    TestFalse(TEXT("ack+ui without spawn"),
        G::AreAllRequiredSet(G::PlayerReadyAck | G::UIInitialized));
    TestFalse(TEXT("ack+spawn without ui"),
        G::AreAllRequiredSet(G::PlayerReadyAck | G::PlayerSpawned));
    TestTrue(TEXT("all required set"),
        G::AreAllRequiredSet(G::PlayerReadyAck | G::UIInitialized | G::PlayerSpawned));
    TestTrue(TEXT("all required plus stats"),
        G::AreAllRequiredSet(G::PlayerReadyAck | G::UIInitialized | G::StatsReceived | G::PlayerSpawned));
    TestTrue(TEXT("extra bits do not block"),
        G::AreAllRequiredSet(0xFF));
    TestEqual(TEXT("mask value pinned"), G::AllRequired, (uint8)0x0B);
    return true;
}
