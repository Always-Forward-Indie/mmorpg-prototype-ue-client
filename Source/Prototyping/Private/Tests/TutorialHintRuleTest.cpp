#include "Misc/AutomationTest.h"
#include "Utils/NetCore/TutorialHintRule.h"

// One-time tutorial copy: distinct non-empty title/body per hint.
// Calls the production FTutorialHintRule used by UTutorialHintManager.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialHintRuleTest, "MMO.Tutorial.HintRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTutorialHintRuleTest::RunTest(const FString& Parameters)
{
    using R = FTutorialHintRule;
    using H = ETutorialHintKind;

    TestFalse(TEXT("menu title"), R::TitleFor(H::PlayerMenu).IsEmpty());
    TestFalse(TEXT("quickbar title"), R::TitleFor(H::Quickbar).IsEmpty());
    TestFalse(TEXT("log title"), R::TitleFor(H::CombatLog).IsEmpty());

    TestFalse(TEXT("menu body"), R::BodyFor(H::PlayerMenu).IsEmpty());
    TestFalse(TEXT("quickbar body"), R::BodyFor(H::Quickbar).IsEmpty());
    TestFalse(TEXT("log body"), R::BodyFor(H::CombatLog).IsEmpty());

    TestNotEqual(TEXT("menu vs quickbar"), R::BodyFor(H::PlayerMenu), R::BodyFor(H::Quickbar));
    TestNotEqual(TEXT("quickbar vs log"), R::BodyFor(H::Quickbar), R::BodyFor(H::CombatLog));

    // Bodies name the gesture/key so the hint is actionable.
    TestTrue(TEXT("menu names right-click"), R::BodyFor(H::PlayerMenu).Contains(TEXT("Right-click")));
    TestTrue(TEXT("quickbar names hotkeys"), R::BodyFor(H::Quickbar).Contains(TEXT("F6")));
    return true;
}
