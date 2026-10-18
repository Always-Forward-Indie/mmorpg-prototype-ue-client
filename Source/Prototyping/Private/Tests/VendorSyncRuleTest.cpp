#include "Misc/AutomationTest.h"
#include "Utils/NetCore/VendorSyncRule.h"

// Vendor/repair shop sync state (B2): open handshake, sell/repair snapshot
// sync, repairable-row pick. Calls the production FVendorSyncRule used by
// the bot vendor legs — no re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVendorSyncRuleTest, "MMO.Vendor.SyncRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVendorSyncRuleTest::RunTest(const FString& Parameters)
{
    using R = FVendorSyncRule;
    using Step = FVendorSyncRule::EOpenStep;

    // Open: snapshot match wins over the sent flag; mismatch asks once, then waits.
    TestEqual(TEXT("match is open"), (int32)R::NextOpenStep(7, 7, false), (int32)Step::ShopOpen);
    TestEqual(TEXT("match open even if sent"), (int32)R::NextOpenStep(7, 7, true), (int32)Step::ShopOpen);
    TestEqual(TEXT("mismatch unsent asks"), (int32)R::NextOpenStep(0, 7, false), (int32)Step::SendOpenRequest);
    TestEqual(TEXT("mismatch sent waits"), (int32)R::NextOpenStep(0, 7, true), (int32)Step::AwaitOpen);
    TestEqual(TEXT("wrong npc waits"), (int32)R::NextOpenStep(9, 7, true), (int32)Step::AwaitOpen);

    // Sell sync: gold up AND stack down by exactly one.
    TestTrue(TEXT("sell synced"), R::IsSellSynced(120, 100, 4, 5));
    TestFalse(TEXT("sell gold flat"), R::IsSellSynced(100, 100, 4, 5));
    TestFalse(TEXT("sell gold down"), R::IsSellSynced(90, 100, 4, 5));
    TestFalse(TEXT("sell qty off by two"), R::IsSellSynced(120, 100, 3, 5));
    TestFalse(TEXT("sell qty up"), R::IsSellSynced(120, 100, 6, 5));

    // Repair sync: durability at max AND gold dropped.
    TestTrue(TEXT("repair synced"), R::IsRepairSynced(100, 100, 80, 100));
    TestFalse(TEXT("repair dur short"), R::IsRepairSynced(90, 100, 80, 100));
    TestFalse(TEXT("repair gold flat"), R::IsRepairSynced(100, 100, 100, 100));
    TestFalse(TEXT("repair gold up"), R::IsRepairSynced(100, 100, 120, 100));

    // Repairable pick: first damaged row with a price; INDEX_NONE = none.
    const TArray<R::FRepairableItem> Empty;
    TestEqual(TEXT("empty is none"), R::FindRepairableIndex(Empty), INDEX_NONE);
    auto Row = [](int32 Id, int32 Max, int32 Cur, int32 Cost)
    {
        R::FRepairableItem R;
        R.InstanceId = Id;
        R.DurabilityMax = Max;
        R.DurabilityCurrent = Cur;
        R.RepairCost = Cost;
        return R;
    };
    const TArray<R::FRepairableItem> Rows = {
        Row(1, 100, 100, 10), // full — skip
        Row(2, 100, 40, 0),   // free — skip
        Row(3, 0, 0, 10),     // no max — skip
        Row(4, 100, 40, 10),  // damaged + priced — pick
        Row(5, 100, 10, 10),  // also damaged, later — not picked
    };
    TestEqual(TEXT("first repairable wins"), R::FindRepairableIndex(Rows), 3);
    const TArray<R::FRepairableItem> Healthy = { Row(1, 100, 100, 10) };
    TestEqual(TEXT("healthy is none"), R::FindRepairableIndex(Healthy), INDEX_NONE);
    return true;
}
