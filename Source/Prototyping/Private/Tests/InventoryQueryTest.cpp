#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryQuery.h"

// Instance-id vs template-id lookups (never mixed) plus case-insensitive
// type/rarity filters. Calls the production FInventoryQuery used by
// UInventoryManager. Instance identity (id) is for row access (tooltips,
// equip, durability); template identity (itemId) is for kind checks
// (use/drop preconditions, quantities).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryQueryTest, "MMO.Inventory.Query",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryQueryTest::RunTest(const FString& Parameters)
{
    using Q = FInventoryQuery;
    TArray<FInventoryItemStruct> Items;
    FInventoryItemStruct Sword; Sword.id = 101; Sword.itemId = 5; Sword.quantity = 3; Sword.type = TEXT("Weapon"); Sword.rarity = TEXT("Rare");
    FInventoryItemStruct Potion; Potion.id = 102; Potion.itemId = 9; Potion.quantity = 7; Potion.type = TEXT("Consumable"); Potion.rarity = TEXT("common");
    FInventoryItemStruct Potion2; Potion2.id = 103; Potion2.itemId = 9; Potion2.quantity = 2; Potion2.type = TEXT("Consumable"); Potion2.rarity = TEXT("common");
    Items.Add(Sword);
    Items.Add(Potion);
    Items.Add(Potion2);

    // Template semantics (kind checks): template id, all matching rows count.
    TestTrue(TEXT("has existing"), Q::HasItem(Items, 5));
    TestFalse(TEXT("missing absent"), Q::HasItem(Items, 42));
    TestEqual(TEXT("quantity sums stacks"), Q::QuantityOf(Items, 9), 9);
    TestEqual(TEXT("quantity missing"), Q::QuantityOf(Items, 42), 0);
    // Instance semantics (row identity): instance id, exact row.
    TestEqual(TEXT("instance found"), Q::IndexOfInstance(Items, 102), 1);
    TestEqual(TEXT("instance missing"), Q::IndexOfInstance(Items, 42), INDEX_NONE);
    TestEqual(TEXT("instance zero rejected"), Q::IndexOfInstance(Items, 0), INDEX_NONE);
    TestEqual(TEXT("template found"), Q::IndexOfTemplate(Items, 9), 1);
    // Instance and template spaces do not collide: instance 9 is not template 9 here.
    TestEqual(TEXT("no cross-match"), Q::IndexOfInstance(Items, 9), INDEX_NONE);
    FInventoryItemStruct Found = Items[Q::IndexOfTemplate(Items, 5)];
    TestEqual(TEXT("by id fields"), Found.quantity, 3);
    TestEqual(TEXT("type filter"), Q::ByType(Items, TEXT("weapon")).Num(), 1);
    TestEqual(TEXT("type case-insensitive"), Q::ByType(Items, TEXT("WEAPON")).Num(), 1);
    TestEqual(TEXT("type miss"), Q::ByType(Items, TEXT("Armor")).Num(), 0);
    TestEqual(TEXT("rarity filter"), Q::ByRarity(Items, TEXT("RARE")).Num(), 1);
    TestEqual(TEXT("empty index"), Q::IndexOfTemplate(TArray<FInventoryItemStruct>(), 5), INDEX_NONE);
    return true;
}
