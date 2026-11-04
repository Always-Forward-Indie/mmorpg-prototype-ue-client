#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryCapacityRule.h"

// Predictive inventory capacity gate: stacking always lands, new templates
// need a free slot, legacy maxSlots<=0 means unlimited.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCapacityRuleTest, "MMO.Items.CapacityRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryCapacityRuleTest::RunTest(const FString& Parameters)
{
    using R = FInventoryCapacityRule;
    // Stacking never needs a new row.
    TestTrue(TEXT("stacking always"), R::CanAcceptNewTemplate(48, 48, true));
    // Legacy unlimited.
    TestTrue(TEXT("legacy unlimited"), R::CanAcceptNewTemplate(1000, 0, false));
    // New row needs a free slot.
    TestTrue(TEXT("free slot"), R::CanAcceptNewTemplate(47, 48, false));
    TestFalse(TEXT("full"), R::CanAcceptNewTemplate(48, 48, false));
    TestFalse(TEXT("overfull"), R::CanAcceptNewTemplate(49, 48, false));
    TestEqual(TEXT("free count"), R::FreeSlots(47, 48), 1);
    TestEqual(TEXT("free zero"), R::FreeSlots(48, 48), 0);
    return true;
}
