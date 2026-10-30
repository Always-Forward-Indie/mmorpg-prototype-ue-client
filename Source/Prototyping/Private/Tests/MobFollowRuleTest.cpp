#include "Misc/AutomationTest.h"
#include "Utils/NetCore/MobFollowRule.h"

// Combat → patrol blend policy and combat freeze gate. Calls production
// FMobFollowRule — no re-implemented math.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobFollowRuleTest, "MMO.Mob.Follow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobFollowRuleTest::RunTest(const FString& Parameters)
{
    using R = FMobFollowRule;

    TestFalse(TEXT("small gap skips blend (no rollback snap)"), R::ShouldBlendOnTransition(15.0f, 40.0f));
    TestFalse(TEXT("boundary gap skips blend"), R::ShouldBlendOnTransition(40.0f, 40.0f));
    TestTrue(TEXT("large gap blends"), R::ShouldBlendOnTransition(200.0f, 40.0f));

    TestTrue(TEXT("blend time clamps to min"),
        FMath::IsNearlyEqual(R::TransitionBlendTime(5.0f, 100.0f, 0.12f, 1.5f), 0.12f));
    TestTrue(TEXT("blend time clamps to max"),
        FMath::IsNearlyEqual(R::TransitionBlendTime(5000.0f, 100.0f, 0.12f, 1.5f), 1.5f));
    TestTrue(TEXT("blend time replays gap at server speed"),
        FMath::IsNearlyEqual(R::TransitionBlendTime(50.0f, 100.0f, 0.12f, 1.5f), 0.5f));

    TestFalse(TEXT("fresh packets never freeze"), R::ShouldFreeze(0.1f, 0.6f));
    TestFalse(TEXT("jitter below cap never freezes"), R::ShouldFreeze(0.59f, 0.6f));
    TestTrue(TEXT("sustained drought freezes"), R::ShouldFreeze(0.7f, 0.6f));
    return true;
}
