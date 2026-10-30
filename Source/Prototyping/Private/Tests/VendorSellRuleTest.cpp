#include "Misc/AutomationTest.h"
#include "Utils/NetCore/VendorSellRule.h"

// Vendor sell-line preview: floor of the tax-adjusted line, min 1g for any
// valued item — 1:1 with chunk VendorManager::resolveSellLine. A single
// animal_bones (sell 1) at 5% tax paid 0 gold before the floor guarantee.
// Calls the production FVendorSellRule used by the vendor shop widgets.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVendorSellRuleTest, "MMO.Shop.SellLine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVendorSellRuleTest::RunTest(const FString& Parameters)
{
    using R = FVendorSellRule;
    // The bone regression: 1g x1 at 5% tax previews (and pays) 1g, never 0.
    TestEqual(TEXT("bone single pays one"), R::LineGold(1, 1, 0.05f), 1);
    TestEqual(TEXT("bone single no tax"), R::LineGold(1, 1, 0.0f), 1);
    // Tax still bites above the floor: 3x potion @4 = 12 -> floor(11.4) = 11.
    TestEqual(TEXT("tax applies"), R::LineGold(4, 3, 0.05f), 11);
    TestEqual(TEXT("no tax raw"), R::LineGold(4, 3, 0.0f), 12);
    // Worthless items stay worthless; invalid input stays zero.
    TestEqual(TEXT("zero unit stays zero"), R::LineGold(0, 5, 0.05f), 0);
    TestEqual(TEXT("zero qty stays zero"), R::LineGold(4, 0, 0.05f), 0);
    TestEqual(TEXT("negative qty stays zero"), R::LineGold(4, -2, 0.05f), 0);
    // Full tax still pays the 1g floor for valued loot.
    TestEqual(TEXT("full tax floor"), R::LineGold(1, 1, 1.0f), 1);
    return true;
}
