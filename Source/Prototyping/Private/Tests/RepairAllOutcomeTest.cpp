#include "Misc/AutomationTest.h"
#include "Utils/NetCore/RepairAllOutcome.h"

// Repair-all outcome: error wins, repaired count decides the rest.
// A hollow success (empty list, zero spent — e.g. no gold on the very first
// item) is NothingToRepair, never Repaired. Calls the production
// FRepairAllOutcome used by URepairShopWidget.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRepairAllOutcomeTest, "MMO.Shop.RepairAll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FRepairAllOutcomeTest::RunTest(const FString& Parameters)
{
    using O = FRepairAllOutcome;
    using E = ERepairAllOutcome;
    TestEqual(TEXT("error wins"), O::Classify(true, 3), E::Failed);
    TestEqual(TEXT("error empty wins"), O::Classify(true, 0), E::Failed);
    TestEqual(TEXT("repaired"), O::Classify(false, 2), E::Repaired);
    TestEqual(TEXT("single counts"), O::Classify(false, 1), E::Repaired);
    TestEqual(TEXT("hollow success"), O::Classify(false, 0), E::NothingToRepair);
    return true;
}
