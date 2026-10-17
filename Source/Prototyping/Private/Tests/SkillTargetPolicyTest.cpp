#include "Misc/AutomationTest.h"
#include "Utils/NetCore/SkillTargetRule.h"

// Auto target-policy derivation (effectType -> policy) + self-cast check.
// Calls the production FSkillTargetRule used by
// UPlayerSkillManager::GetEffectiveTargetPolicy — the manager keeps the
// skill lookup and the explicit-policy override.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillTargetPolicyTest, "MMO.Skills.TargetPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillTargetPolicyTest::RunTest(const FString& Parameters)
{
    using R = FSkillTargetRule;
    using P = ESkillTargetPolicy;
    using E = ESkillEffectType;
    TestEqual(TEXT("damage"), R::PolicyForEffect(E::Damage), P::TargetOnly);
    TestEqual(TEXT("debuff"), R::PolicyForEffect(E::Debuff), P::TargetOnly);
    TestEqual(TEXT("healing"), R::PolicyForEffect(E::Healing), P::SelfAndTarget);
    TestEqual(TEXT("buff"), R::PolicyForEffect(E::Buff), P::SelfAndTarget);
    TestEqual(TEXT("resource"), R::PolicyForEffect(E::Resource), P::SelfAndTarget);
    TestEqual(TEXT("teleport"), R::PolicyForEffect(E::Teleport), P::SelfOnly);
    TestEqual(TEXT("none"), R::PolicyForEffect(E::None), P::SelfOnly);
    TestTrue(TEXT("selfonly castable"), R::IsSelfCastable(P::SelfOnly));
    TestTrue(TEXT("selfandtarget castable"), R::IsSelfCastable(P::SelfAndTarget));
    TestFalse(TEXT("targetonly not self"), R::IsSelfCastable(P::TargetOnly));
    TestFalse(TEXT("auto not self"), R::IsSelfCastable(P::Auto));
    return true;
}
