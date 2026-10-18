#include "Misc/AutomationTest.h"
#include "Utils/NetCore/JoinGuard.h"

// Double-join guard for JoinSelectedCharacterToGame (P0.2): the first call
// wins, concurrent/repeated calls are ignored until Reset. Calls the
// production FJoinGuard used by UMyGameInstance — no re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoinGuardTest, "MMO.Network.JoinGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FJoinGuardTest::RunTest(const FString& Parameters)
{
    FJoinGuard Fresh;
    TestFalse(TEXT("fresh not entered"), Fresh.IsEntered());
    TestTrue(TEXT("first caller wins"), Fresh.TryEnter());
    TestTrue(TEXT("latched after win"), Fresh.IsEntered());
    TestFalse(TEXT("second caller ignored"), Fresh.TryEnter());
    TestTrue(TEXT("still latched"), Fresh.IsEntered());

    // Reset re-arms for the next explicit join (travel settled/cancelled).
    Fresh.Reset();
    TestFalse(TEXT("reset clears"), Fresh.IsEntered());
    TestTrue(TEXT("post-reset caller wins"), Fresh.TryEnter());

    // Double-reset is harmless (idempotent cancel path).
    Fresh.Reset();
    Fresh.Reset();
    TestFalse(TEXT("double reset clear"), Fresh.IsEntered());
    return true;
}
