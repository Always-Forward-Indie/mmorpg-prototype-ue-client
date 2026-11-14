#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventorySelectionRule.h"

// LMB single-select: plain click selects, re-click clears, empty clears.
// Every expectation calls the production rule — no re-implemented logic.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventorySelectionRuleTest, "MMO.UI.InventorySelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventorySelectionRuleTest::RunTest(const FString& Parameters)
{
    using R = FInventorySelectionRule;
    TestEqual(TEXT("select empty->slot"), R::Resolve(INDEX_NONE, 3, true), 3);
    TestEqual(TEXT("toggle off"), R::Resolve(3, 3, true), INDEX_NONE);
    TestEqual(TEXT("move selection"), R::Resolve(3, 5, true), 5);
    TestEqual(TEXT("empty clears"), R::Resolve(3, 4, false), INDEX_NONE);
    TestEqual(TEXT("empty from none"), R::Resolve(INDEX_NONE, 4, false), INDEX_NONE);
    TestEqual(TEXT("invalid keeps"), R::Resolve(2, INDEX_NONE, true), 2);
    TestTrue(TEXT("valid"), R::IsValid(0, 48));
    TestFalse(TEXT("invalid negative"), R::IsValid(INDEX_NONE, 48));
    TestFalse(TEXT("invalid overflow"), R::IsValid(48, 48));
    return true;
}
