#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ChatWhisperRule.h"

// Whisper echo policy: only whispers need a local echo (server never echoes
// them to the sender; zone/local return via broadcast). Calls production
// FChatWhisperRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChatWhisperRuleTest, "MMO.Chat.WhisperEcho",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FChatWhisperRuleTest::RunTest(const FString& Parameters)
{
    using R = FChatWhisperRule;
    TestTrue(TEXT("whisper echoes"), R::NeedsLocalEcho(TEXT("whisper")));
    TestTrue(TEXT("whisper case-insensitive"), R::NeedsLocalEcho(TEXT("Whisper")));
    TestFalse(TEXT("zone returns via broadcast"), R::NeedsLocalEcho(TEXT("zone")));
    TestFalse(TEXT("local returns via broadcast"), R::NeedsLocalEcho(TEXT("local")));
    TestFalse(TEXT("empty never echoes"), R::NeedsLocalEcho(TEXT("")));
    return true;
}
