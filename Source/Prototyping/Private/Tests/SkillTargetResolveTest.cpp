#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SkillTargetRule.h"

// Full target-resolution decision table (inputs pass through untouched on
// failure). Calls the production FSkillTargetRule::Resolve used by
// UPlayerSkillManager::ResolveSkillTarget — the manager keeps the policy
// lookup and the log lines.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillTargetResolveTest, "MMO.Skills.TargetResolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillTargetResolveTest::RunTest(const FString& Parameters)
{
    using R = FSkillTargetRule;
    using P = ESkillTargetPolicy;
    using O = ETargetResolveOutcome;
    using T = ECasterType;
    // SelfOnly always targets self.
    FSkillTargetResolution Self = R::Resolve(P::SelfOnly, 7, T::Mob, 3);
    TestEqual(TEXT("self outcome"), Self.Outcome, O::OkSelf);
    TestEqual(TEXT("self id"), Self.TargetId, 3);
    TestEqual(TEXT("self type"), Self.TargetType, T::Self);
    // TargetOnly needs an external target.
    FSkillTargetResolution Missing = R::Resolve(P::TargetOnly, 0, T::None, 3);
    TestEqual(TEXT("missing outcome"), Missing.Outcome, O::MissingTarget);
    TestEqual(TEXT("missing id untouched"), Missing.TargetId, 0);
    FSkillTargetResolution SelfShot = R::Resolve(P::TargetOnly, 3, T::Self, 3);
    TestEqual(TEXT("self shot outcome"), SelfShot.Outcome, O::MissingTarget);
    FSkillTargetResolution Ext = R::Resolve(P::TargetOnly, 7, T::Mob, 3);
    TestEqual(TEXT("external outcome"), Ext.Outcome, O::OkExternal);
    TestEqual(TEXT("external id kept"), Ext.TargetId, 7);
    // SelfAndTarget prefers external, falls back to self.
    FSkillTargetResolution Both = R::Resolve(P::SelfAndTarget, 7, T::Mob, 3);
    TestEqual(TEXT("both outcome"), Both.Outcome, O::OkExternal);
    FSkillTargetResolution Fallback = R::Resolve(P::SelfAndTarget, 0, T::None, 3);
    TestEqual(TEXT("fallback outcome"), Fallback.Outcome, O::OkFallback);
    TestEqual(TEXT("fallback id"), Fallback.TargetId, 3);
    TestEqual(TEXT("fallback type"), Fallback.TargetType, T::Self);
    // Unknown policy fails closed.
    FSkillTargetResolution Unknown = R::Resolve(P::Auto, 7, T::Mob, 3);
    TestEqual(TEXT("unknown outcome"), Unknown.Outcome, O::UnknownPolicy);
    return true;
}
