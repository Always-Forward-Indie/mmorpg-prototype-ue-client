#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ThrowawayNameRule.h"

// Throwaway Qabot names (slice 2): quest/level run on a fresh char instead
// of the fixed 614/615 pair (one-shot quest 9000, exp cap at 10). Names must
// satisfy the login-server contract (letters + single spaces only), so the
// timestamp seed is base-26 encoded, never digits. Calls production
// FThrowawayNameRule — no local re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThrowawayNameRuleTest, "MMO.Auth.ThrowawayName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FThrowawayNameRuleTest::RunTest(const FString& Parameters)
{
    using R = FThrowawayNameRule;
    TestEqual(TEXT("prefix"), R::Prefix(), TEXT("Qabot "));
    TestEqual(TEXT("suffix len"), R::SuffixLen(), 4);

    // Deterministic encoding: same seed, same suffix.
    TestEqual(TEXT("encode 0"), R::EncodeSuffix(0), TEXT("AAAA"));
    TestEqual(TEXT("encode 1"), R::EncodeSuffix(1), TEXT("BAAA"));
    TestEqual(TEXT("encode 26"), R::EncodeSuffix(26), TEXT("ABAA"));
    TestEqual(TEXT("suffix is letters"), R::EncodeSuffix(123456).Len(), 4);

    // Full names are valid char names and recognized as ours.
    const FString N0 = R::MakeQabotName(0);
    TestEqual(TEXT("make 0"), N0, TEXT("Qabot AAAA"));
    TestTrue(TEXT("is qabot"), R::IsQabotName(N0));
    TestTrue(TEXT("valid name"), R::IsValidCharName(N0));
    TestTrue(TEXT("seed 1 valid"), R::IsValidCharName(R::MakeQabotName(1)));
    TestTrue(TEXT("seed 456k valid"), R::IsValidCharName(R::MakeQabotName(456976)));

    // Rejections: wrong prefix, digits, underscores, bad spacing/length.
    TestFalse(TEXT("test v not qabot"), R::IsQabotName(TEXT("Test V")));
    TestFalse(TEXT("digits rejected"), R::IsQabotName(TEXT("Qabot 1234")));
    TestFalse(TEXT("underscore rejected"), R::IsValidCharName(TEXT("Qabot_ABCD")));
    TestFalse(TEXT("digits invalid"), R::IsValidCharName(TEXT("Qabot 1234")));
    TestFalse(TEXT("leading space"), R::IsValidCharName(TEXT(" Qabot AAAA")));
    TestFalse(TEXT("trailing space"), R::IsValidCharName(TEXT("Qabot AAAA ")));
    TestFalse(TEXT("double space"), R::IsValidCharName(TEXT("Qabot  AAAA")));
    TestFalse(TEXT("too short"), R::IsValidCharName(TEXT("A")));
    return true;
}
