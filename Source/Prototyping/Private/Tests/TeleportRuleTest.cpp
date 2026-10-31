#include "Misc/AutomationTest.h"
#include "Math/NumericLimits.h"
#include "Utils/NetCore/TeleportRule.h"
#include <limits>

// Teleport cover-transition policy: fade/hold durations, cover + far
// thresholds, target sanity (finite + in-bounds), yaw normalization.
// Calls the production FTeleportRule used by UTeleportManager/ABasicPlayer.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeleportRuleTest, "MMO.Teleport.Rule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTeleportRuleTest::RunTest(const FString& Parameters)
{
    using R = FTeleportRule;
    TestEqual(TEXT("fade out"), R::FadeOutSec, 0.22f);
    TestEqual(TEXT("hold"), R::HoldSec, 0.15f);
    TestEqual(TEXT("fade in"), R::FadeInSec, 0.30f);
    TestEqual(TEXT("total cover"), R::CoverTotalSec(), 0.22f + 0.15f + 0.30f);

    TestFalse(TEXT("small step needs no cover"), R::NeedsCover(499.9f));
    TestTrue(TEXT("snap threshold covered"), R::NeedsCover(500.0f));
    TestFalse(TEXT("mid-range is not far"), R::IsFarTeleport(4999.9f));
    TestTrue(TEXT("far threshold"), R::IsFarTeleport(5000.0f));
    TestEqual(TEXT("near hold"), R::HoldForDistance(1000.0f), 0.15f);
    TestEqual(TEXT("far hold extended"), R::HoldForDistance(6000.0f), 0.15f + 0.35f);
    TestEqual(TEXT("correction tail"), R::CorrectionTailSec, 1.0f);

    TestTrue(TEXT("spawn valid"), R::IsValidTarget(700.0, -3200.0, 200.0, 90.0));
    TestFalse(TEXT("NaN rejected"), R::IsValidTarget(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0));
    TestFalse(TEXT("Inf rejected"), R::IsValidTarget(std::numeric_limits<double>::infinity(), 0.0, 0.0, 0.0));
    TestFalse(TEXT("absurd X rejected"), R::IsValidTarget(99999999.0, 0.0, 0.0, 0.0));
    TestFalse(TEXT("absurd Z rejected"), R::IsValidTarget(0.0, 0.0, -99999999.0, 0.0));

    TestEqual(TEXT("yaw passthrough"), R::NormalizeYaw(90.0f), 90.0f);
    TestEqual(TEXT("yaw wraps"), R::NormalizeYaw(270.0f), -90.0f);
    TestEqual(TEXT("yaw negative wraps"), R::NormalizeYaw(-270.0f), 90.0f);
    TestEqual(TEXT("yaw 180 stable"), R::NormalizeYaw(180.0f), -180.0f);
    return true;
}
