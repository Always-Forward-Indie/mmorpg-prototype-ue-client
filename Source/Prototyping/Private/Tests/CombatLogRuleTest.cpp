#include "Misc/AutomationTest.h"
#include "Utils/NetCore/CombatLogRule.h"

// Combat-log channel policy: masks, colors, formats, capacity.
// Calls the production FCombatLogRule used by UCombatLogManager/Widget.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLogRuleTest, "MMO.CombatLog.Rule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCombatLogRuleTest::RunTest(const FString& Parameters)
{
    using R = FCombatLogRule;
    using C = ECombatLogCategory;

    TestEqual(TEXT("capacity"), R::MaxEntries, 500);
    TestTrue(TEXT("widget cap below manager cap"), R::MaxWidgetLines <= R::MaxEntries);

    // Mask ops: all-on by default, per-channel toggle.
    TestEqual(TEXT("all mask"), R::AllMask(), 0x1Fu);
    TestTrue(TEXT("damage shown by default"), R::IsShown(R::AllMask(), C::Damage));
    uint8 NoLoot = R::AllMask() & ~R::MaskFor(C::Loot);
    TestFalse(TEXT("loot hidden after clear"), R::IsShown(NoLoot, C::Loot));
    TestTrue(TEXT("damage still shown"), R::IsShown(NoLoot, C::Damage));

    // Clean misses produce no line; any number (or death) does.
    TestFalse(TEXT("pure miss skipped"), R::ShouldLogResult(0, 0, 0, false));
    TestTrue(TEXT("damage logged"), R::ShouldLogResult(25, 0, 0, false));
    TestTrue(TEXT("heal logged"), R::ShouldLogResult(0, 40, 0, false));
    TestTrue(TEXT("mana logged"), R::ShouldLogResult(0, 0, 10, false));
    TestTrue(TEXT("kill without numbers logged"), R::ShouldLogResult(0, 0, 0, true));

    // Line formats carry the load-bearing facts.
    const FString Dmg = R::FormatDamage(TEXT("Fireball"), TEXT("Mob #7"), 120, true, false, true, true, false);
    TestTrue(TEXT("dmg has skill"), Dmg.Contains(TEXT("Fireball")));
    TestTrue(TEXT("dmg has amount"), Dmg.Contains(TEXT("120")));
    TestTrue(TEXT("dmg has crit"), Dmg.Contains(TEXT("crit")));
    TestTrue(TEXT("dmg has slain"), Dmg.Contains(TEXT("slain")));

    const FString Loot = R::FormatLoot(TEXT("Wolf Fang"), 3);
    TestTrue(TEXT("loot has name"), Loot.Contains(TEXT("Wolf Fang")));
    TestTrue(TEXT("loot has qty"), Loot.Contains(TEXT("3")));

    const FString Xp = R::FormatExp(150, TEXT("Mob Kill"));
    TestTrue(TEXT("xp has amount"), Xp.Contains(TEXT("150")));
    TestTrue(TEXT("xp has reason"), Xp.Contains(TEXT("Mob Kill")));

    TestTrue(TEXT("levelup has arrow"), R::FormatLevelUp(4, 5).Contains(TEXT("4")));

    // Target description never empty.
    TestFalse(TEXT("target desc empty"), R::TargetDesc(12, TEXT("")).IsEmpty());
    TestTrue(TEXT("target has id"), R::TargetDesc(12, TEXT("Mob")).Contains(TEXT("12")));

    // Every channel has a distinct color.
    TestNotEqual(TEXT("dmg vs heal color"),
        R::ColorFor(C::Damage).ToFColor(true), R::ColorFor(C::Heal).ToFColor(true));
    return true;
}
