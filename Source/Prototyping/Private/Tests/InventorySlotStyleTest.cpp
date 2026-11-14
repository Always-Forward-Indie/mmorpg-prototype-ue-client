#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventorySlotStyleRule.h"

// Slot state machine: explicit priority, single style key per combination.
// Every expectation below calls the production rule — no re-implemented logic.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventorySlotStyleTest, "MMO.UI.InventorySlotStyle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventorySlotStyleTest::RunTest(const FString& Parameters)
{
    using R = FInventorySlotStyleRule;
    TestEqual(TEXT("empty base"), R::FrameKey(false, false, false, false), FString(TEXT("empty")));
    TestEqual(TEXT("filled base"), R::FrameKey(true, false, false, false), FString(TEXT("filled")));
    TestEqual(TEXT("hovered"), R::FrameKey(true, false, false, true), FString(TEXT("hovered")));
    TestEqual(TEXT("hover empty"), R::FrameKey(false, false, false, true), FString(TEXT("hovered")));
    // Explicit priority: highlight beats selected beats hover.
    TestEqual(TEXT("selected beats hover"), R::FrameKey(true, true, false, true), FString(TEXT("selected")));
    TestEqual(TEXT("highlight beats selected"), R::FrameKey(true, true, true, true), FString(TEXT("highlighted")));
    TestEqual(TEXT("highlight beats hover"), R::FrameKey(true, false, true, true), FString(TEXT("highlighted")));
    TestEqual(TEXT("highlight on empty"), R::FrameKey(false, false, true, false), FString(TEXT("highlighted")));
    // Rarity tints: fixed constants, case-insensitive, white fallback.
    TestTrue(TEXT("common white"), R::RarityColor(TEXT("common")).Equals(FLinearColor::White));
    TestTrue(TEXT("uncommon green"), R::RarityColor(TEXT("Uncommon")).Equals(FLinearColor::Green));
    TestTrue(TEXT("rare blue"), R::RarityColor(TEXT("rare")).Equals(FLinearColor::Blue));
    TestTrue(TEXT("magic blue"), R::RarityColor(TEXT("magic")).Equals(FLinearColor::Blue));
    TestTrue(TEXT("epic purple"), R::RarityColor(TEXT("epic")).Equals(FLinearColor(0.5f, 0.0f, 1.0f)));
    TestTrue(TEXT("legendary orange"), R::RarityColor(TEXT("legendary")).Equals(FLinearColor(1.0f, 0.5f, 0.0f)));
    TestTrue(TEXT("mythic red"), R::RarityColor(TEXT("mythic")).Equals(FLinearColor::Red));
    TestTrue(TEXT("unique yellow"), R::RarityColor(TEXT("unique")).Equals(FLinearColor(1.0f, 1.0f, 0.0f)));
    TestTrue(TEXT("unknown white"), R::RarityColor(TEXT("whatever")).Equals(FLinearColor::White));
    TestTrue(TEXT("empty white"), R::RarityColor(TEXT("")).Equals(FLinearColor::White));
    return true;
}
