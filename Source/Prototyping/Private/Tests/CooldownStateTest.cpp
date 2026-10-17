#include "Misc/AutomationTest.h"
#include "Utils/NetCore/CooldownFormat.h"

// Cooldown active flag: on cooldown while now is strictly before the end
// time (equality = ready). Calls the production FCooldownState used by
// UPlayerSkillManager::UpdateSkillCooldownState.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCooldownStateTest, "MMO.Skills.CooldownState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCooldownStateTest::RunTest(const FString& Parameters)
{
    using S = FCooldownState;
    TestTrue(TEXT("before end"), S::IsOnCooldown(10.0f, 20.0f));
    TestFalse(TEXT("equality is ready"), S::IsOnCooldown(20.0f, 20.0f));
    TestFalse(TEXT("after end"), S::IsOnCooldown(25.0f, 20.0f));
    return true;
}
