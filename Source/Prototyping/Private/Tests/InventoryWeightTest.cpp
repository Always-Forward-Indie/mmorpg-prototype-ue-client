#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryWeight.h"

// Carried-weight sum (weight x quantity per row) and encumbrance buckets
// (>=100% overweight, >=80% heavy, unknown max never alarms). Calls the
// production FInventoryWeight used by UInventoryWidget.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryWeightTest, "MMO.Inventory.Weight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryWeightTest::RunTest(const FString& Parameters)
{
    using W = FInventoryWeight;
    using S = EWeightStatus;
    TArray<FInventoryItemStruct> Empty;
    TestEqual(TEXT("empty weighs zero"), W::Total(Empty), 0.0f);

    FInventoryItemStruct A; A.weight = 2.5f; A.quantity = 4;
    FInventoryItemStruct B; B.weight = 1.0f; B.quantity = 1;
    TestEqual(TEXT("quantity multiplies"), W::Total({ A, B }), 11.0f);

    TestEqual(TEXT("ok"), W::StatusFor(50.0f, 100.0f), S::Ok);
    TestEqual(TEXT("heavy boundary"), W::StatusFor(80.0f, 100.0f), S::Heavy);
    TestEqual(TEXT("over boundary"), W::StatusFor(100.0f, 100.0f), S::Overweight);
    TestEqual(TEXT("over max"), W::StatusFor(150.0f, 100.0f), S::Overweight);
    TestEqual(TEXT("unknown max never alarms"), W::StatusFor(999.0f, 0.0f), S::Ok);
    return true;
}
