#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryCategoryRule.h"

// Category buckets derive from server-contract flags; multi-select is OR;
// all-set and none-set both read as All. Calls the production rule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCategoryRuleTest, "MMO.UI.InventoryCategory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

static FInventoryItemStruct MakeCatItem(bool bEquip, const FString& EquipSlot, bool bUsable, bool bQuest)
{
    FInventoryItemStruct Item;
    Item.itemId = 1;
    Item.isEquippable = bEquip;
    Item.equipSlotSlug = EquipSlot;
    Item.isUsable = bUsable;
    Item.isQuestItem = bQuest;
    return Item;
}

bool FInventoryCategoryRuleTest::RunTest(const FString& Parameters)
{
    using R = FInventoryCategoryRule;
    using C = EInventoryCategory;
    const FInventoryItemStruct Sword = MakeCatItem(true, TEXT(""), false, false);
    const FInventoryItemStruct SlotEq = MakeCatItem(false, TEXT("main_hand"), false, false);
    const FInventoryItemStruct Potion = MakeCatItem(false, TEXT(""), true, false);
    const FInventoryItemStruct Relic = MakeCatItem(false, TEXT(""), false, true);
    const FInventoryItemStruct Ore = MakeCatItem(false, TEXT(""), false, false);
    const FInventoryItemStruct Hybrid = MakeCatItem(true, TEXT(""), true, false);

    TestTrue(TEXT("equip by bool"), R::Matches(Sword, C::Equipment));
    TestTrue(TEXT("equip by slot"), R::Matches(SlotEq, C::Equipment));
    TestTrue(TEXT("usable"), R::Matches(Potion, C::Usable));
    TestTrue(TEXT("quest"), R::Matches(Relic, C::Quest));
    TestTrue(TEXT("other"), R::Matches(Ore, C::Other));
    TestFalse(TEXT("other excludes equip"), R::Matches(Sword, C::Other));
    TestFalse(TEXT("other excludes usable"), R::Matches(Potion, C::Other));

    // Overlap: hybrid passes under either bucket (OR semantics).
    TestTrue(TEXT("hybrid via equip"), R::PassesFilter(Hybrid, true, false, false, false));
    TestTrue(TEXT("hybrid via usable"), R::PassesFilter(Hybrid, false, true, false, false));
    TestFalse(TEXT("hybrid hidden by quest-only"), R::PassesFilter(Hybrid, false, false, true, false));

    // All-set and none-set both read as All (grid never empties by construction).
    TestTrue(TEXT("all set passes"), R::PassesFilter(Ore, true, true, true, true));
    TestTrue(TEXT("none set passes"), R::PassesFilter(Ore, false, false, false, false));
    TestFalse(TEXT("equip-only hides ore"), R::PassesFilter(Ore, true, false, false, false));
    TestTrue(TEXT("other-only shows ore"), R::PassesFilter(Ore, false, false, false, true));
    return true;
}
