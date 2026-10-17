#include "Misc/AutomationTest.h"
#include "Utils/NetCore/VendorAffordability.h"

// Vendor affordability: level gate first (more informative), then gold
// minus cart commitments. Player-facing predicate only — the server
// re-validates everything authoritatively. Calls the production
// FVendorAffordability used by UVendorShopWidget.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVendorAffordabilityTest, "MMO.Shop.Affordability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVendorAffordabilityTest::RunTest(const FString& Parameters)
{
    using A = FVendorAffordability;
    using E = EVendorSlotAffordability;
    TestEqual(TEXT("affordable"), A::ForShopItem(5, 5, 100, 500, 0), E::Affordable);
    // Level gate wins over gold (more informative to the player).
    TestEqual(TEXT("level first"), A::ForShopItem(3, 5, 10000, 500, 0), E::LevelTooLow);
    TestEqual(TEXT("gold short"), A::ForShopItem(5, 5, 600, 500, 0), E::NotEnoughGold);
    // Cart commitments reduce available gold.
    TestEqual(TEXT("cart eats gold"), A::ForShopItem(5, 1, 100, 500, 450), E::NotEnoughGold);
    TestEqual(TEXT("cart fits"), A::ForShopItem(5, 1, 100, 500, 400), E::Affordable);
    // Unknown level (0, stats not in yet) never blocks on level.
    TestEqual(TEXT("unknown level skips gate"), A::ForShopItem(0, 99, 100, 500, 0), E::Affordable);
    TestEqual(TEXT("cart over"), A::ForCartEntry(501, 500), E::NotEnoughGold);
    TestEqual(TEXT("cart exact"), A::ForCartEntry(500, 500), E::Affordable);
    TestEqual(TEXT("cart under"), A::ForCartEntry(100, 500), E::Affordable);
    return true;
}
