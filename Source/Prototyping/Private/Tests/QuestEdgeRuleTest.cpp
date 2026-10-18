#include "Misc/AutomationTest.h"
#include "Utils/NetCore/QuestEdgeRule.h"

// Quest dialogue edge picking (P0.2 reconnect proved the transport; B1 pins
// the quest predicate): first matching edge wins, 0 = none. Calls the
// production FQuestEdgeRule used by UDevModeBotBrain — no re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestEdgeRuleTest, "MMO.Quest.EdgeRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
FQuestEdgeRule::FChoice MakeEdge(int32 Id, bool bQuest, bool bTurnin, bool bMet, const TCHAR* Key)
{
    FQuestEdgeRule::FChoice E;
    E.EdgeId = Id;
    E.bHasQuestPreview = bQuest;
    E.bHasTurnInPreview = bTurnin;
    E.bConditionMet = bMet;
    E.ChoiceKey = Key;
    return E;
}
}

bool FQuestEdgeRuleTest::RunTest(const FString& Parameters)
{
    using R = FQuestEdgeRule;
    const TArray<R::FChoice> Empty;
    TestEqual(TEXT("empty goal is 0"), R::FindGoalEdge(Empty, false), 0);
    TestEqual(TEXT("empty advance is 0"), R::FindAdvanceEdge(Empty), 0);

    // Offer picked when its condition is met; unmet offer skipped.
    const TArray<R::FChoice> Offer = { MakeEdge(11, true, false, true, TEXT("take_quest")) };
    TestEqual(TEXT("offer edge found"), R::FindGoalEdge(Offer, false), 11);
    const TArray<R::FChoice> LockedOffer = { MakeEdge(11, true, false, false, TEXT("take_quest")) };
    TestEqual(TEXT("locked offer skipped"), R::FindGoalEdge(LockedOffer, false), 0);

    // Turn-in flag switches which preview counts.
    const TArray<R::FChoice> Both = {
        MakeEdge(11, true, false, true, TEXT("take_quest")),
        MakeEdge(12, false, true, true, TEXT("hand_in")),
    };
    TestEqual(TEXT("accept leg picks offer"), R::FindGoalEdge(Both, false), 11);
    TestEqual(TEXT("turnin leg picks turnin"), R::FindGoalEdge(Both, true), 12);

    // First match wins, even when a later edge also matches.
    const TArray<R::FChoice> TwoOffers = {
        MakeEdge(11, true, false, true, TEXT("take_a")),
        MakeEdge(12, true, false, true, TEXT("take_b")),
    };
    TestEqual(TEXT("first offer wins"), R::FindGoalEdge(TwoOffers, false), 11);

    // Advance: neutral met edge passes; farewell-class never advances.
    const TArray<R::FChoice> Hub = {
        MakeEdge(21, false, false, false, TEXT("continue")),
        MakeEdge(22, false, false, true, TEXT("Farewell")),
        MakeEdge(23, false, false, true, TEXT("ask_more")),
    };
    TestEqual(TEXT("advance skips unmet + farewell"), R::FindAdvanceEdge(Hub), 23);
    TestTrue(TEXT("farewell upper"), R::IsFarewellKey(TEXT("FAREWELL")));
    TestTrue(TEXT("farewell mixed"), R::IsFarewellKey(TEXT("GoodBye")));
    TestTrue(TEXT("bye"), R::IsFarewellKey(TEXT("byE")));
    TestTrue(TEXT("leave"), R::IsFarewellKey(TEXT("please_leave")));
    TestTrue(TEXT("goodbye"), R::IsFarewellKey(TEXT("say_goodbye_now")));
    TestTrue(TEXT("close"), R::IsFarewellKey(TEXT("CloseWindow")));
    TestTrue(TEXT("exit"), R::IsFarewellKey(TEXT("EXIT_dialogue")));
    TestTrue(TEXT("cancel word"), R::IsFarewellKey(TEXT("Cancel")));
    TestFalse(TEXT("continue is not farewell"), R::IsFarewellKey(TEXT("continue")));
    TestFalse(TEXT("empty key is not farewell"), R::IsFarewellKey(TEXT("")));

    // Goal edges have priority in the CALLER, not here: advance returns the
    // first met non-farewell edge even if it carries a quest preview.
    const TArray<R::FChoice> GoalFirst = {
        MakeEdge(11, true, false, true, TEXT("take_quest")),
        MakeEdge(23, false, false, true, TEXT("ask_more")),
    };
    TestEqual(TEXT("advance may return goal edge"), R::FindAdvanceEdge(GoalFirst), 11);
    return true;
}
