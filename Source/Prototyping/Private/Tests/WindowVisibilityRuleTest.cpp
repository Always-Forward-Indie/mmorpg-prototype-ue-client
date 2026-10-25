#include "Misc/AutomationTest.h"
#include "Utils/NetCore/WindowVisibilityRule.h"

// UI window visibility policy (S07): HasUIWindowOpen is any-bit-set,
// HasModalWindowOpen is the Dialogue|Trade|HarvestLoot subset. Calls the
// production FWindowVisibilityRule used by UUIManager.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindowVisibilityRuleTest, "MMO.UI.WindowVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FWindowVisibilityRuleTest::RunTest(const FString& Parameters)
{
    using R = FWindowVisibilityRule;
    TestFalse(TEXT("nothing open"), R::HasUIWindowOpen(0));
    TestFalse(TEXT("nothing modal"), R::HasModalWindowOpen(0));
    TestTrue(TEXT("inventory counts"), R::HasUIWindowOpen(R::Inventory));
    TestFalse(TEXT("inventory not modal"), R::HasModalWindowOpen(R::Inventory));
    TestTrue(TEXT("stats not modal"), R::HasUIWindowOpen(R::PlayerStats) && !R::HasModalWindowOpen(R::PlayerStats));
    TestTrue(TEXT("dialogue modal"), R::HasModalWindowOpen(R::Dialogue));
    TestTrue(TEXT("trade modal"), R::HasModalWindowOpen(R::Trade));
    TestTrue(TEXT("harvest loot modal"), R::HasModalWindowOpen(R::HarvestLoot));
    TestTrue(TEXT("mixed includes modal"),
        R::HasModalWindowOpen(R::Inventory | R::Bestiary | R::Trade));
    TestTrue(TEXT("modal mask"),
        R::ModalMask == (R::Dialogue | R::Trade | R::HarvestLoot));
    TestTrue(TEXT("all windows open counts"),
        R::HasUIWindowOpen(0x7FFFu));
    TestTrue(TEXT("all windows include modal"),
        R::HasModalWindowOpen(0x7FFFu));
    return true;
}
