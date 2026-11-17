#include "Misc/AutomationTest.h"
#include "Utils/NetCore/PointedProgressCore.h"

// Pointed <=====> bar math: defined-corner hull SDF predicates (aspect-aware,
// height-normalized bar space) plus ghost-trail stepping. Calls production
// FPointedProgressCore used by UPointedProgressWidget and mirrored 1:1 by
// M_UI_ProgressPointed (material adds only SmoothStep AA).

namespace
{
	constexpr float TestAspect = 8.0f;      // 400x50 reference widget
	constexpr float TestTipLenH = 0.36f;    // 18px tip on a 50px-tall bar
	constexpr float TestBorderH = 0.04f;    // 2px border on a 50px-tall bar
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointedProgressCoreTest, "MMO.UI.PointedProgress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPointedProgressCoreTest::RunTest(const FString& Parameters)
{
    using R = FPointedProgressCore;
    // Clamp.
    TestEqual(TEXT("clamp low"), R::Clamp01(-0.5f), 0.0f);
    TestEqual(TEXT("clamp high"), R::Clamp01(1.7f), 1.0f);
    // Tip parameter: 0 at the nose, 1 once TipLenH inside.
    TestEqual(TEXT("tip nose"), R::TipT(0.0f, TestTipLenH, TestAspect), 0.0f);
    TestEqual(TEXT("tip far end"), R::TipT(1.0f, TestTipLenH, TestAspect), 0.0f);
    TestEqual(TEXT("tip center"), R::TipT(0.5f, TestTipLenH, TestAspect), 1.0f);
    TestEqual(TEXT("tip no taper"), R::TipT(0.0f, 0.0f, TestAspect), 1.0f);
    // Hull: parabolic nose, 0 at tips, full height inside, smooth junction.
    TestEqual(TEXT("half nose"), R::ShapeHalfHeight(0.0f, TestTipLenH, TestAspect), 0.0f);
    TestEqual(TEXT("half center"), R::ShapeHalfHeight(0.5f, TestTipLenH, TestAspect), 0.5f);
    // Junction (edgeH == tipLenH) meets the straight edge exactly at 0.5:
    // min(U, 1-U) = TipLenH / Aspect, i.e. U = TipLenH / Aspect near a tip.
    TestEqual(TEXT("half junction"), R::ShapeHalfHeight(TestTipLenH / TestAspect, TestTipLenH, TestAspect), 0.5f);
    // Nose neighborhood stays smooth and positive (no needle, no flat zero zone).
    const float NoseH = R::ShapeHalfHeight(0.005f, TestTipLenH, TestAspect);
    TestTrue(TEXT("nose positive"), NoseH > 0.07f && NoseH < 0.08f);
    TestTrue(TEXT("inside center"), R::IsInsideShape(0.5f, 0.5f, TestTipLenH, TestAspect));
    // Center top edge is exactly on the hull boundary (<= is inclusive, the
    // shader centers its SmoothStep AA there, so the edge pixel is half-alpha).
    TestTrue(TEXT("center top edge inclusive"), R::IsInsideShape(0.5f, 1.0f, TestTipLenH, TestAspect));
    // Near a tip the parabolic hull tapers: (0.01, 0.9) is outside (dy 0.4 > h ~0.2).
    TestFalse(TEXT("outside near tip"), R::IsInsideShape(0.01f, 0.9f, TestTipLenH, TestAspect));
    // Fill: vertical wipe, hull stays intact.
    TestTrue(TEXT("filled left"), R::IsFilled(0.2f, 0.5f));
    TestFalse(TEXT("empty right"), R::IsFilled(0.8f, 0.5f));
    // Border: uniform inset, degenerate tip returns 0 instead of NaN.
    TestEqual(TEXT("inner tip"), R::InnerHalfHeight(0.0f, TestTipLenH, TestAspect, TestBorderH), 0.0f);
    TestTrue(TEXT("inner inside"), R::InnerHalfHeight(0.5f, TestTipLenH, TestAspect, TestBorderH) > 0.4f);
    // Ghost: converges toward target, heals snap via widget (ghost < progress).
    const float Stepped = R::StepGhost(1.0f, 0.5f, 0.1f, 3.0f);
    TestTrue(TEXT("ghost falls"), Stepped < 1.0f && Stepped >= 0.5f);
    TestEqual(TEXT("ghost settled"), R::StepGhost(0.5f, 0.5f, 0.1f, 3.0f), 0.5f);
    return true;
}
