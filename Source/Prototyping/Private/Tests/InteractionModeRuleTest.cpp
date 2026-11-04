#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InteractionModeRule.h"

// BDO-style cursor-xor-crosshair: cursor visibility is the single source of truth.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionModeRuleTest, "MMO.Targeting.InteractionMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInteractionModeRuleTest::RunTest(const FString& Parameters)
{
    using R = FInteractionModeRule;
    TestEqual(TEXT("cursor mode"), (int32)R::Resolve(true), (int32)EInteractionMode::Cursor);
    TestEqual(TEXT("combat mode"), (int32)R::Resolve(false), (int32)EInteractionMode::Combat);
    TestFalse(TEXT("no crosshair with cursor"), R::ShouldShowCrosshair(true, false));
    TestTrue(TEXT("crosshair in combat"), R::ShouldShowCrosshair(false, false));
    TestFalse(TEXT("no crosshair when dead"), R::ShouldShowCrosshair(false, true));
    TestTrue(TEXT("shoulder applied"), R::ResolveShoulderOffset(true, FVector(0.f, 90.f, 30.f)).Equals(FVector(0.f, 90.f, 30.f)));
    TestTrue(TEXT("shoulder off"), R::ResolveShoulderOffset(false, FVector(0.f, 90.f, 30.f)).IsZero());
    return true;
}
