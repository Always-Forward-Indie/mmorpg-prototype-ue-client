#include "Misc/AutomationTest.h"
#include "Utils/NetCore/CooldownFormat.h"

// Skill-slot cooldown text: tenths under a second, whole seconds under a
// minute, m:ss beyond. Calls the production FCooldownFormat used by
// USkillSlotWidget — no local re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCooldownFormatTest, "MMO.UI.CooldownFormat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCooldownFormatTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("tenths"), FCooldownFormat::Format(0.5f), FString(TEXT("0.5")));
    TestEqual(TEXT("tenths 2"), FCooldownFormat::Format(0.7f), FString(TEXT("0.7")));
    TestEqual(TEXT("whole seconds"), FCooldownFormat::Format(5.0f), FString(TEXT("5")));
    TestEqual(TEXT("rounds down"), FCooldownFormat::Format(59.4f), FString(TEXT("59")));
    TestEqual(TEXT("minute boundary"), FCooldownFormat::Format(61.0f), FString(TEXT("1:01")));
    TestEqual(TEXT("minutes padded"), FCooldownFormat::Format(125.0f), FString(TEXT("2:05")));
    return true;
}
