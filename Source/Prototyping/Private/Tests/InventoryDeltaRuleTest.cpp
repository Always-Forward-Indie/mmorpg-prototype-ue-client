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
    // Slim delta never wipes enriched statics: a locally enriched row
    // (slug/name from harvest pickup) survives the slim INVENTORY_UPDATE.
    {
        FInventoryItemStruct Enriched = Row(0, 63, 1);
        Enriched.slug = TEXT("undead_dust");
        Enriched.itemSlug = TEXT("undead_dust");
        Enriched.name = TEXT("Undead Dust");
        TArray<FInventoryItemStruct> Items = { Enriched };
        R::ApplyInventoryDelta(Items, { Row(0, 63, 1) }, {});
        TestEqual(TEXT("still one"), Items.Num(), 1);
        TestEqual(TEXT("slug kept"), Items[0].slug, FString(TEXT("undead_dust")));
        TestEqual(TEXT("name kept"), Items[0].name, FString(TEXT("Undead Dust")));
    }
    // Slim id-sync keeps statics while adopting the assigned instance id.
    {
        FInventoryItemStruct Enriched = Row(0, 63, 1);
        Enriched.slug = TEXT("undead_dust");
        Enriched.name = TEXT("Undead Dust");
        TArray<FInventoryItemStruct> Items = { Enriched };
        FInventoryItemStruct Synced = Row(600, 63, 1);
        R::ApplyInventoryDelta(Items, { Synced }, {});
        TestEqual(TEXT("no duplicate on sync"), Items.Num(), 1);
        TestEqual(TEXT("id adopted"), Items[0].id, 600);
        TestEqual(TEXT("slug kept on sync"), Items[0].slug, FString(TEXT("undead_dust")));
        TestEqual(TEXT("name kept on sync"), Items[0].name, FString(TEXT("Undead Dust")));
    }
    // Slim delta never regresses an assigned id to 0.
    {
        FInventoryItemStruct Assigned = Row(600, 63, 1);
        Assigned.slug = TEXT("undead_dust");
        TArray<FInventoryItemStruct> Items = { Assigned };
        // A legacy id==0 slim row for the same template is added alongside
        // (existing rule) and must not touch the assigned row.
        R::ApplyInventoryDelta(Items, { Row(0, 63, 2) }, {});
        TestEqual(TEXT("two rows"), Items.Num(), 2);
        TestEqual(TEXT("assigned id kept"), Items[0].id, 600);
        TestEqual(TEXT("assigned slug kept"), Items[0].slug, FString(TEXT("undead_dust")));
    }
    // Full row (statics present) replaces wholesale, incl. renames.
    {
        FInventoryItemStruct Enriched = Row(600, 63, 1);
        Enriched.slug = TEXT("old_slug");
        Enriched.name = TEXT("Old Name");
        TArray<FInventoryItemStruct> Items = { Enriched };
        FInventoryItemStruct Full = Row(600, 63, 3);
        Full.slug = TEXT("undead_dust");
        Full.name = TEXT("Undead Dust");
        R::ApplyInventoryDelta(Items, { Full }, {});
        TestEqual(TEXT("one row after full"), Items.Num(), 1);
        TestEqual(TEXT("qty from full"), Items[0].quantity, 3);
        TestEqual(TEXT("name from full"), Items[0].name, FString(TEXT("Undead Dust")));
        TestEqual(TEXT("itemSlug mirrored"), Items[0].itemSlug, FString(TEXT("undead_dust")));
    }
    return true;
}
