#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryRules.h"

// Durability fallback rule: explicit 0 from the server means broken and is
// never masked; missing field falls back to max only for durable items with
// sane max. Calls the production FInventoryRules used by
// JSONParser::DeserializeInventoryItem (also covered end-to-end by
// MMO.Protocol.DurabilityZeroKept).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDurabilityRuleTest, "MMO.Inventory.DurabilityRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDurabilityRuleTest::RunTest(const FString& Parameters)
{
    using R = FInventoryRules;
    TestEqual(TEXT("explicit zero kept"), R::ResolveDurabilityCurrent(true, 0, 100, true), 0);
    TestEqual(TEXT("missing falls back to max"), R::ResolveDurabilityCurrent(false, 0, 100, true), 100);
    TestEqual(TEXT("non-durable stays zero"), R::ResolveDurabilityCurrent(false, 0, 100, false), 0);
    TestEqual(TEXT("zero max stays zero"), R::ResolveDurabilityCurrent(false, 0, 0, true), 0);
    TestEqual(TEXT("partial damage kept"), R::ResolveDurabilityCurrent(false, 30, 100, true), 30);
    TestEqual(TEXT("sent value kept"), R::ResolveDurabilityCurrent(true, 50, 100, true), 50);
    return true;
}
