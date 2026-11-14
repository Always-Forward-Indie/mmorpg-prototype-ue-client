#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryGridRule.h"

// Grid capacity: server maxSlots wins, 0 keeps the local fallback size.
// Rows grow to fit, columns stay fixed. Calls the production rule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryGridRuleTest, "MMO.UI.InventoryGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryGridRuleTest::RunTest(const FString& Parameters)
{
    using R = FInventoryGridRule;
    TestEqual(TEXT("server wins"), R::DesiredTotal(30, 48), 30);
    TestEqual(TEXT("legacy fallback"), R::DesiredTotal(0, 48), 48);
    TestEqual(TEXT("rows exact"), R::RowsForTotal(48, 8), 6);
    TestEqual(TEXT("rows partial"), R::RowsForTotal(30, 8), 4);
    TestTrue(TEXT("needs resize"), R::NeedsResize(48, 30, 48));
    TestFalse(TEXT("converged"), R::NeedsResize(48, 0, 48));
    return true;
}
