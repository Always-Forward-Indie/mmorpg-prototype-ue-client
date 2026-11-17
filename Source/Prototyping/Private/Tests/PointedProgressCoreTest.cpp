#include "Misc/AutomationTest.h"
#include "Utils/NetCore/PointedProgressCore.h"

// Pointed <=====> bar math: hull SDF predicates plus ghost-trail stepping.
// Calls production FPointedProgressCore used by UPointedProgressWidget and
// mirrored 1:1 by M_UI_ProgressPointed (material adds only SmoothStep AA).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointedProgressCoreTest, "MMO.UI.PointedProgress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPointedProgressCoreTest::RunTest(const FString& Parameters)
{
    using R = FPointedProgressCore;
    // Clamp.
    TestEqual(TEXT("clamp low"), R::Clamp01(-0.5f), 0.0f);
    TestEqual(TEXT("clamp high"), R::Clamp01(1.7f), 1.0f);
    // Edge ramp: 0 at exact tips, 1 once Tip width inside.
    TestEqual(TEXT("ramp left tip"), R::EdgeRamp(0.0f, 0.12f), 0.0f);
    TestEqual(TEXT("ramp right tip"), R::EdgeRamp(1.0f, 0.12f), 0.0f);
    TestEqual(TEXT("ramp center"), R::EdgeRamp(0.5f, 0.12f), 1.0f);
    TestEqual(TEXT("ramp no tip"), R::EdgeRamp(0.0f, 0.0f), 1.0f);
    // Hull: pointed ends collapse, middle is full height.
    TestEqual(TEXT("half tip"), R::ShapeHalfHeight(0.0f, 0.12f), 0.0f);
    TestEqual(TEXT("half center"), R::ShapeHalfHeight(0.5f, 0.12f), 0.5f);
    TestTrue(TEXT("inside center"), R::IsInsideShape(0.5f, 0.5f, 0.12f));
    // Center top edge is exactly on the hull boundary (<= is inclusive, the
    // shader centers its SmoothStep AA there, so the edge pixel is half-alpha).
    TestTrue(TEXT("center top edge inclusive"), R::IsInsideShape(0.5f, 1.0f, 0.12f));
    // Near a tip the hull tapers: (0.06, 0.9) is outside (dy 0.4 > half 0.25).
    TestFalse(TEXT("outside near tip"), R::IsInsideShape(0.06f, 0.9f, 0.12f));
    TestFalse(TEXT("tip row empty"), R::IsInsideShape(0.0f, 0.5f + 0.01f, 0.12f));
    // Fill: vertical wipe, hull stays intact.
    TestTrue(TEXT("filled left"), R::IsFilled(0.2f, 0.5f));
    TestFalse(TEXT("empty right"), R::IsFilled(0.8f, 0.5f));
    // Border: degenerate tip returns 0 instead of NaN.
    TestEqual(TEXT("inner degenerate"), R::InnerRamp(0.5f, 0.03f, 0.03f), 0.0f);
    TestTrue(TEXT("inner inside"), R::InnerHalfHeight(0.5f, 0.12f, 0.03f) > 0.0f);
    // Ghost: converges toward target, heals snap via widget (ghost < progress).
    const float Stepped = R::StepGhost(1.0f, 0.5f, 0.1f, 3.0f);
    TestTrue(TEXT("ghost falls"), Stepped < 1.0f && Stepped >= 0.5f);
    TestEqual(TEXT("ghost settled"), R::StepGhost(0.5f, 0.5f, 0.1f, 3.0f), 0.5f);
    return true;
}
