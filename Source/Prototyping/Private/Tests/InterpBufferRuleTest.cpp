#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InterpBufferRule.h"

// Snapshot interpolation buffer contract (S10): render delay from the server
// interval, time-ordered insert with late-drop and cap eviction, bracketing
// sample with hold-newest/hold-oldest. Calls production FInterpBufferRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInterpBufferRuleTest, "MMO.Move.InterpBuffer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

static FInterpSnapshot Snap(double T, float X)
{
    FInterpSnapshot S;
    S.TimeSec = T; S.X = X;
    return S;
}

bool FInterpBufferRuleTest::RunTest(const FString& Parameters)
{
    using R = FInterpBufferRule;
    TestEqual(TEXT("delay nominal"), R::RenderDelaySec(0.1), 0.15);
    TestEqual(TEXT("delay floor"), R::RenderDelaySec(0.0), 0.1);
    TestEqual(TEXT("delay ceiling"), R::RenderDelaySec(1.0), 0.25);

    TArray<FInterpSnapshot> Buf;
    TestEqual(TEXT("insert first"), R::InsertSorted(Buf, Snap(1.0, 0.f)), 1);
    TestEqual(TEXT("insert second"), R::InsertSorted(Buf, Snap(1.1, 10.f)), 2);
    TestEqual(TEXT("late dropped"), R::InsertSorted(Buf, Snap(1.05, 5.f)), 2);
    TestEqual(TEXT("duplicate time dropped"), R::InsertSorted(Buf, Snap(1.1, 11.f)), 2);
    for (int32 i = 2; i < 12; ++i)
    {
        R::InsertSorted(Buf, Snap(1.0 + i * 0.1, float(i * 10)));
    }
    TestEqual(TEXT("capacity capped"), Buf.Num(), R::Capacity);
    TestTrue(TEXT("oldest evicted"), Buf[0].TimeSec >= 1.3);

    int32 A = -1, B = -1;
    float Alpha = -1.f;
    TestFalse(TEXT("empty no sample"), R::Sample(TArray<FInterpSnapshot>(), 1.0, A, B, Alpha));
    TArray<FInterpSnapshot> One;
    R::InsertSorted(One, Snap(1.0, 0.f));
    TestFalse(TEXT("single no sample"), R::Sample(One, 1.0, A, B, Alpha));

    TArray<FInterpSnapshot> Two;
    R::InsertSorted(Two, Snap(1.0, 0.f));
    R::InsertSorted(Two, Snap(1.2, 20.f));
    TestTrue(TEXT("mid samples"), R::Sample(Two, 1.1, A, B, Alpha));
    TestEqual(TEXT("mid A"), A, 0);
    TestEqual(TEXT("mid B"), B, 1);
    TestTrue(TEXT("mid alpha"), FMath::IsNearlyEqual(Alpha, 0.5f, 0.001f));
    TestTrue(TEXT("hold newest"), R::Sample(Two, 5.0, A, B, Alpha));
    TestEqual(TEXT("hold newest idx"), A, B);
    TestEqual(TEXT("hold newest is last"), A, 1);
    TestTrue(TEXT("hold oldest"), R::Sample(Two, 0.0, A, B, Alpha));
    TestEqual(TEXT("hold oldest is first"), A, 0);
    TestTrue(TEXT("exact newest"), R::Sample(Two, 1.2, A, B, Alpha));
    return true;
}
