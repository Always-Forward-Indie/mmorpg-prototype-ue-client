#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryQuery.h"

// First-match inventory lookups (missing rows: index -1, default struct,
// zero quantity) and case-insensitive type/rarity filters. Calls the
// production FInventoryQuery used by UInventoryManager.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryQueryTest, "MMO.Inventory.Query",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryQueryTest::RunTest(const FString& Parameters)
{
    using Q = FInventoryQuery;
    TArray<FInventoryItemStruct> Items;
    FInventoryItemStruct Sword; Sword.itemId = 5; Sword.quantity = 3; Sword.type = TEXT("Weapon"); Sword.rarity = TEXT("Rare");
    FInventoryItemStruct Potion; Potion.itemId = 9; Potion.quantity = 7; Potion.type = TEXT("Consumable"); Potion.rarity = TEXT("common");
    Items.Add(Sword);
    Items.Add(Potion);

    TestTrue(TEXT("has existing"), Q::HasItem(Items, 5));
    TestFalse(TEXT("missing absent"), Q::HasItem(Items, 42));
    TestEqual(TEXT("index found"), Q::IndexOf(Items, 9), 1);
    TestEqual(TEXT("index missing"), Q::IndexOf(Items, 42), INDEX_NONE);
    TestEqual(TEXT("quantity found"), Q::QuantityOf(Items, 5), 3);
    TestEqual(TEXT("quantity missing"), Q::QuantityOf(Items, 42), 0);
    FInventoryItemStruct Found = Items[Q::IndexOf(Items, 5)];
    TestEqual(TEXT("by id fields"), Found.quantity, 3);
    TestEqual(TEXT("type filter"), Q::ByType(Items, TEXT("weapon")).Num(), 1);
    TestEqual(TEXT("type case-insensitive"), Q::ByType(Items, TEXT("WEAPON")).Num(), 1);
    TestEqual(TEXT("type miss"), Q::ByType(Items, TEXT("Armor")).Num(), 0);
    TestEqual(TEXT("rarity filter"), Q::ByRarity(Items, TEXT("RARE")).Num(), 1);
    TestEqual(TEXT("empty index"), Q::IndexOf(TArray<FInventoryItemStruct>(), 5), INDEX_NONE);
    return true;
}
