#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SlotMath.h"

// Row-major grid mapping (index = Row * Columns + Column, inverse by
// division). A non-positive column count can never yield a valid index
// (the original code would divide by zero) — it yields -1 / (0, 0).
// Calls the production FSlotMath used by UInventoryWidget.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlotMathTest, "MMO.Inventory.SlotMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSlotMathTest::RunTest(const FString& Parameters)
{
    using M = FSlotMath;
    TestEqual(TEXT("origin"), M::IndexFromPosition(0, 0, 8), 0);
    TestEqual(TEXT("row major"), M::IndexFromPosition(2, 3, 8), 19);
    TestEqual(TEXT("zero columns"), M::IndexFromPosition(2, 3, 0), INDEX_NONE);
    int32 Row = -1, Col = -1;
    M::PositionFromIndex(19, 8, Row, Col);
    TestEqual(TEXT("row back"), Row, 2);
    TestEqual(TEXT("col back"), Col, 3);
    M::PositionFromIndex(0, 8, Row, Col);
    TestEqual(TEXT("origin back row"), Row, 0);
    TestEqual(TEXT("origin back col"), Col, 0);
    M::PositionFromIndex(19, 0, Row, Col);
    TestEqual(TEXT("zero columns row"), Row, 0);
    TestEqual(TEXT("zero columns col"), Col, 0);
    // Round-trip over a full grid.
    for (int32 i = 0; i < 48; ++i)
    {
        int32 R = -1, C = -1;
        M::PositionFromIndex(i, 8, R, C);
        TestEqual(TEXT("round trip"), M::IndexFromPosition(R, C, 8), i);
    }
    return true;
}
