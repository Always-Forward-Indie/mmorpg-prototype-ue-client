#include "Misc/AutomationTest.h"
#include "Utils/NetCore/MobTargetFrameRule.h"

// Rank / level-chrome rules for the mob target frame (S-slice 1).
// Every expectation below calls the production rule — no re-implemented logic.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobTargetFrameRankTest, "MMO.UI.MobTargetFrameRank",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobTargetFrameRankTest::RunTest(const FString& Parameters)
{
    using R = FMobTargetRankRule;
    TestEqual(TEXT("normal kept"), R::NormalizeRankCode(TEXT("normal"), false), FString(TEXT("normal")));
    TestEqual(TEXT("elite kept"), R::NormalizeRankCode(TEXT("elite"), false), FString(TEXT("elite")));
    TestEqual(TEXT("boss kept"), R::NormalizeRankCode(TEXT("boss"), false), FString(TEXT("boss")));
    TestEqual(TEXT("champion kept"), R::NormalizeRankCode(TEXT("champion"), true), FString(TEXT("champion")));
    // Champion flag wins over a stale/legacy code.
    TestEqual(TEXT("flag promotes normal"), R::NormalizeRankCode(TEXT("normal"), true), FString(TEXT("champion")));
    // Case-insensitive server codes.
    TestEqual(TEXT("upper elite"), R::NormalizeRankCode(TEXT("ELITE"), false), FString(TEXT("elite")));
    // Unknown codes degrade to normal — the rank row is never hidden for a mob.
    TestEqual(TEXT("unknown degrades"), R::NormalizeRankCode(TEXT("rare_plus"), false), FString(TEXT("normal")));
    // Empty code = non-mob target (player/self) -> hidden, never "normal".
    TestTrue(TEXT("empty hidden"), R::NormalizeRankCode(TEXT(""), false).IsEmpty());
    TestTrue(TEXT("normal shown"), R::ShouldShowRankLabel(TEXT("normal")));
    TestTrue(TEXT("elite shown"), R::ShouldShowRankLabel(TEXT("elite")));
    TestTrue(TEXT("champion shown"), R::ShouldShowRankLabel(TEXT("champion")));
    TestTrue(TEXT("boss shown"), R::ShouldShowRankLabel(TEXT("boss")));
    TestFalse(TEXT("empty hidden flag"), R::ShouldShowRankLabel(TEXT("")));
    // Border colours are fixed constants (frame is never localized).
    TestTrue(TEXT("boss orange"), R::BorderColorForRank(TEXT("boss")).Equals(FLinearColor(1.0f, 0.45f, 0.10f)));
    TestTrue(TEXT("champion purple"), R::BorderColorForRank(TEXT("champion")).Equals(FLinearColor(0.70f, 0.30f, 1.0f)));
    TestTrue(TEXT("elite blue"), R::BorderColorForRank(TEXT("elite")).Equals(FLinearColor(0.20f, 0.50f, 1.0f)));
    TestTrue(TEXT("normal gray"), R::BorderColorForRank(TEXT("normal")).Equals(FLinearColor(0.55f, 0.55f, 0.55f)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobTargetFrameLevelTest, "MMO.UI.MobTargetFrameLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobTargetFrameLevelTest::RunTest(const FString& Parameters)
{
    using L = FMobTargetLevelRule;
    // diff = viewer - mob. Lethal at -5 and below, always with the marker.
    auto StyleMinus6 = L::StyleForDiff(10, 16);
    TestTrue(TEXT("lethal red"), StyleMinus6.Color.Equals(FLinearColor(1.0f, 0.20f, 0.20f)));
    TestTrue(TEXT("lethal marker"), StyleMinus6.bLethalMarker);
    auto StyleMinus5 = L::StyleForDiff(10, 15);
    TestTrue(TEXT("minus5 lethal"), StyleMinus5.bLethalMarker);
    auto StyleMinus4 = L::StyleForDiff(10, 14);
    TestTrue(TEXT("minus4 orange"), StyleMinus4.Color.Equals(FLinearColor(1.0f, 0.55f, 0.10f)));
    TestFalse(TEXT("minus4 no marker"), StyleMinus4.bLethalMarker);
    auto StyleMinus3 = L::StyleForDiff(10, 13);
    TestTrue(TEXT("minus3 orange"), StyleMinus3.Color.Equals(FLinearColor(1.0f, 0.55f, 0.10f)));
    auto StyleEven = L::StyleForDiff(10, 10);
    TestTrue(TEXT("even white"), StyleEven.Color.Equals(FLinearColor::White));
    TestFalse(TEXT("even no marker"), StyleEven.bLethalMarker);
    auto StylePlus2 = L::StyleForDiff(12, 10);
    TestTrue(TEXT("plus2 white"), StylePlus2.Color.Equals(FLinearColor::White));
    auto StylePlus3 = L::StyleForDiff(13, 10);
    TestTrue(TEXT("plus3 green"), StylePlus3.Color.Equals(FLinearColor(0.30f, 1.0f, 0.30f)));
    auto StylePlus5 = L::StyleForDiff(15, 10);
    TestTrue(TEXT("plus5 green"), StylePlus5.Color.Equals(FLinearColor(0.30f, 1.0f, 0.30f)));
    auto StylePlus6 = L::StyleForDiff(16, 10);
    TestTrue(TEXT("plus6 gray"), StylePlus6.Color.Equals(FLinearColor(0.55f, 0.55f, 0.60f)));
    // Unknown viewer level degrades to neutral white (never red, never marker).
    auto StyleNoViewer = L::StyleForDiff(0, 50);
    TestTrue(TEXT("no viewer white"), StyleNoViewer.Color.Equals(FLinearColor::White));
    TestFalse(TEXT("no viewer no marker"), StyleNoViewer.bLethalMarker);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobTargetFrameCastTest, "MMO.UI.MobTargetFrameCast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobTargetFrameCastTest::RunTest(const FString& Parameters)
{
    using C = FMobTargetCastRule;
    // Visibility gate: instant and basic_attack never show.
    TestFalse(TEXT("instant hidden"), C::ShouldShowCastbar(0.0f, TEXT("fireball")));
    TestFalse(TEXT("basic hidden"), C::ShouldShowCastbar(1.5f, TEXT("basic_attack")));
    TestFalse(TEXT("basic upper hidden"), C::ShouldShowCastbar(1.5f, TEXT("BASIC_ATTACK")));
    TestTrue(TEXT("cast shown"), C::ShouldShowCastbar(1.5f, TEXT("fireball")));
    TestTrue(TEXT("tiny cast shown"), C::ShouldShowCastbar(0.05f, TEXT("fireball")));
    // Server-time progress: halfway through a 2 s cast started at T.
    TestEqual(TEXT("half progress"), C::ProgressAt(1000, 0, 2.0f), 0.5f);
    TestEqual(TEXT("start zero"), C::ProgressAt(0, 0, 2.0f), 0.0f);
    TestEqual(TEXT("end one"), C::ProgressAt(2000, 0, 2.0f), 1.0f);
    TestEqual(TEXT("clamped past end"), C::ProgressAt(5000, 0, 2.0f), 1.0f);
    TestEqual(TEXT("clamped before start"), C::ProgressAt(-500, 0, 2.0f), 0.0f);
    TestEqual(TEXT("remaining half"), C::RemainingSecAt(1000, 0, 2.0f), 1.0f);
    TestEqual(TEXT("remaining floored"), C::RemainingSecAt(5000, 0, 2.0f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobTargetFrameEffectsTest, "MMO.UI.MobTargetFrameEffects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobTargetFrameEffectsTest::RunTest(const FString& Parameters)
{
    using S = FMobTargetEffectSort;
    TestEqual(TEXT("cap six"), S::MaxVisibleEffects, 6);
    TestEqual(TEXT("shield first"), S::PriorityForSlug(TEXT("fire_shield")), 0);
    TestEqual(TEXT("invuln second"), S::PriorityForSlug(TEXT("invulnerability")), 1);
    TestEqual(TEXT("untargetable second"), S::PriorityForSlug(TEXT("untargetable_buff")), 1);
    TestEqual(TEXT("enrage third"), S::PriorityForSlug(TEXT("blood_enrage")), 2);
    TestEqual(TEXT("plain last"), S::PriorityForSlug(TEXT("poison_dot")), 3);
    TestEqual(TEXT("empty slug last"), S::PriorityForSlug(TEXT("")), 3);
    // Priority beats time; within a tier shortest remaining wins.
    TestTrue(TEXT("shield beats poison"), S::EffectLess(TEXT("shield"), 30.0f, TEXT("poison_dot"), 1.0f));
    TestFalse(TEXT("poison loses to shield"), S::EffectLess(TEXT("poison_dot"), 1.0f, TEXT("shield"), 30.0f));
    TestTrue(TEXT("shorter first"), S::EffectLess(TEXT("poison_dot"), 2.0f, TEXT("burn_dot"), 9.0f));
    TestFalse(TEXT("longer later"), S::EffectLess(TEXT("burn_dot"), 9.0f, TEXT("poison_dot"), 2.0f));
    return true;
}
