#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryRules.h"

// S2.3 delta merge: changed[] upserts by instance id, removed[] drops rows,
// quantity<=0 vanishes, id-sync (id>0 onto pending id==0) updates in place.
// Calls production FInventoryRules::ApplyInventoryDelta directly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryDeltaRuleTest, "MMO.Inventory.DeltaRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryDeltaRuleTest::RunTest(const FString& Parameters)
{
    using R = FInventoryRules;
    auto Row = [](int32 Id, int32 ItemId, int32 Qty)
    {
        FInventoryItemStruct S;
        S.id = Id;
        S.itemId = ItemId;
        S.quantity = Qty;
        return S;
    };

    // Update by id.
    {
        TArray<FInventoryItemStruct> Items = { Row(101, 46, 2) };
        R::ApplyInventoryDelta(Items, { Row(101, 46, 5) }, {});
        TestEqual(TEXT("qty updated"), Items.Num(), 1);
        TestEqual(TEXT("qty value"), Items[0].quantity, 5);
    }
    // Removal by id + zero-qty vanish.
    {
        TArray<FInventoryItemStruct> Items = { Row(101, 46, 2), Row(102, 47, 1) };
        R::ApplyInventoryDelta(Items, { Row(102, 47, 0) }, { 101 });
        TestEqual(TEXT("both gone"), Items.Num(), 0);
    }
    // Id-sync: id>0 onto pending id==0 same template updates in place.
    {
        TArray<FInventoryItemStruct> Items = { Row(0, 46, 1) };
        FInventoryItemStruct Synced = Row(0, 46, 1);
        Synced.id = 201;
        R::ApplyInventoryDelta(Items, { Synced }, {});
        TestEqual(TEXT("no duplicate"), Items.Num(), 1);
        TestEqual(TEXT("id synced"), Items[0].id, 201);
    }
    // Pre-token id==0 never overwrites an assigned id: a new pending instance
    // is added alongside, the assigned row stays intact.
    {
        TArray<FInventoryItemStruct> Items = { Row(201, 46, 1) };
        R::ApplyInventoryDelta(Items, { Row(0, 46, 9) }, {});
        TestEqual(TEXT("added alongside"), Items.Num(), 2);
        TestEqual(TEXT("assigned id kept"), Items[0].id, 201);
        TestEqual(TEXT("assigned qty kept"), Items[0].quantity, 1);
    }
    // Unknown id>0 row is added.
    {
        TArray<FInventoryItemStruct> Items = { Row(101, 46, 2) };
        R::ApplyInventoryDelta(Items, { Row(202, 48, 1) }, {});
        TestEqual(TEXT("added"), Items.Num(), 2);
    }
    // Unknown removal id is a no-op.
    {
        TArray<FInventoryItemStruct> Items = { Row(101, 46, 2) };
        R::ApplyInventoryDelta(Items, {}, { 999 });
        TestEqual(TEXT("untouched"), Items.Num(), 1);
    }
    return true;
}
