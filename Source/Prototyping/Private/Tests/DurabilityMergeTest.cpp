#include "Misc/AutomationTest.h"
#include "Utils/NetCore/DurabilityMerge.h"

// Single-channel durability apply: match by instance id, clamp into range,
// mismatch is a no-op. Calls production FDurabilityMerge used by
// UEquipmentManager::ApplyDurabilityUpdate.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDurabilityMergeTest, "MMO.Equipment.DurabilityMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDurabilityMergeTest::RunTest(const FString& Parameters)
{
    using M = FDurabilityMerge;

    TestEqual(TEXT("clamp negative"), M::ClampCurrent(-5, 100), 0);
    TestEqual(TEXT("clamp over max"), M::ClampCurrent(150, 100), 100);
    TestEqual(TEXT("keep in range"), M::ClampCurrent(40, 100), 40);
    TestEqual(TEXT("unknown max passes through"), M::ClampCurrent(40, 0), 40);

    FEquipmentSlotData Slot;
    Slot.bIsOccupied = true;
    Slot.inventoryItemId = 7;
    Slot.durabilityCurrent = 80;
    Slot.durabilityMax = 100;
    Slot.isDurabilityWarning = false;
    TestTrue(TEXT("slot match"), M::ApplyToSlot(Slot, 7, 30, 100, true));
    TestEqual(TEXT("slot patched"), Slot.durabilityCurrent, 30);
    TestTrue(TEXT("slot warning"), Slot.isDurabilityWarning);

    FEquipmentSlotData Other;
    Other.bIsOccupied = true;
    Other.inventoryItemId = 9;
    Other.durabilityCurrent = 80;
    TestFalse(TEXT("slot mismatch"), M::ApplyToSlot(Other, 7, 30, 100, true));
    TestEqual(TEXT("slot untouched"), Other.durabilityCurrent, 80);

    FEquipmentSlotData Empty;
    Empty.bIsOccupied = false;
    Empty.inventoryItemId = 7;
    TestFalse(TEXT("empty slot skipped"), M::ApplyToSlot(Empty, 7, 30, 100, false));

    FInventoryItemStruct Item;
    Item.id = 7;
    Item.durabilityCurrent = 80;
    TestTrue(TEXT("item match"), M::ApplyToItem(Item, 7, 30));
    TestEqual(TEXT("item patched"), Item.durabilityCurrent, 30);
    TestFalse(TEXT("item mismatch"), M::ApplyToItem(Item, 8, 10));
    return true;
}
