#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ProximityRule.h"

// Nearest-within-range contract for registry-backed proximity queries:
// IsInRange is an inclusive radius check; SelectNearestIndex returns the
// smallest qualifying index or INDEX_NONE. Calls the production
// FProximityRule used by the Inventory/Harvest proximity helpers.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProximityRuleTest, "MMO.Registry.Proximity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProximityRuleTest::RunTest(const FString& Parameters)
{
    using R = FProximityRule;
    TestTrue(TEXT("inside range"), R::IsInRange(100.0f, 180.0f));
    TestTrue(TEXT("boundary inclusive"), R::IsInRange(180.0f, 180.0f));
    TestFalse(TEXT("outside range"), R::IsInRange(180.1f, 180.0f));
    TestTrue(TEXT("zero radius exact"), R::IsInRange(0.0f, 0.0f));

    TestEqual(TEXT("empty -> none"), R::SelectNearestIndex({}, 500.0f), INDEX_NONE);
    TestEqual(TEXT("all outside -> none"), R::SelectNearestIndex({600.0f, 700.0f}, 500.0f), INDEX_NONE);
    TestEqual(TEXT("picks nearest"), R::SelectNearestIndex({300.0f, 100.0f, 200.0f}, 500.0f), 1);
    TestEqual(TEXT("boundary counts"), R::SelectNearestIndex({500.0f, 600.0f}, 500.0f), 0);
    TestEqual(TEXT("ties keep first"), R::SelectNearestIndex({150.0f, 150.0f}, 500.0f), 0);
    TestEqual(TEXT("single inside"), R::SelectNearestIndex({42.0f}, 100.0f), 0);
    return true;
}
