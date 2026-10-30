#include "Misc/AutomationTest.h"
#include "Utils/NetCore/ToastCoalesceRule.h"

// Toast coalescing policy: identical gains inside the window merge, losses
// and one-shots never merge. Calls production FToastCoalesceRule.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToastCoalesceRuleTest, "MMO.Toast.Coalesce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FToastCoalesceRuleTest::RunTest(const FString& Parameters)
{
    using R = FToastCoalesceRule;
    TestTrue(TEXT("window positive"), R::CoalesceWindowSec() > 0.0f);

    // Gains toast, losses do not.
    TestTrue(TEXT("item gain"), R::IsGainType(TEXT("item_received")));
    TestTrue(TEXT("gold gain"), R::IsGainType(TEXT("gold_received")));
    TestTrue(TEXT("exp gain"), R::IsGainType(TEXT("exp_received")));
    TestFalse(TEXT("drop loss"), R::IsGainType(TEXT("item_dropped")));
    TestFalse(TEXT("quest fail loss"), R::IsGainType(TEXT("quest_failed")));
    TestFalse(TEXT("zone info"), R::IsGainType(TEXT("zone_explored")));

    // Same slug merges, different slugs do not.
    TMap<FString, FString> Bone3, Bone2, Potion;
    Bone3.Add(TEXT("itemSlug"), TEXT("animal_bone")); Bone3.Add(TEXT("quantity"), TEXT("3"));
    Bone2.Add(TEXT("itemSlug"), TEXT("animal_bone")); Bone2.Add(TEXT("quantity"), TEXT("2"));
    Potion.Add(TEXT("itemSlug"), TEXT("health_potion")); Potion.Add(TEXT("quantity"), TEXT("1"));
    TestEqual(TEXT("same slug same key"),
        R::CoalesceKey(TEXT("item_received"), Bone3), R::CoalesceKey(TEXT("item_received"), Bone2));
    TestNotEqual(TEXT("different slug different key"),
        R::CoalesceKey(TEXT("item_received"), Bone3), R::CoalesceKey(TEXT("item_received"), Potion));

    // Legacy item_slug spelling keys identically.
    TMap<FString, FString> Legacy;
    Legacy.Add(TEXT("item_slug"), TEXT("animal_bone")); Legacy.Add(TEXT("quantity"), TEXT("1"));
    TestEqual(TEXT("legacy slug spelling merges"),
        R::CoalesceKey(TEXT("item_received"), Bone3), R::CoalesceKey(TEXT("item_received"), Legacy));

    // itemId fallback when no slug is present.
    TMap<FString, FString> IdA, IdB;
    IdA.Add(TEXT("itemId"), TEXT("67")); IdB.Add(TEXT("itemId"), TEXT("68"));
    TestNotEqual(TEXT("different ids differ"),
        R::CoalesceKey(TEXT("item_received"), IdA), R::CoalesceKey(TEXT("item_received"), IdB));

    // Gold/exp merge across sources; one-shots never merge.
    TestEqual(TEXT("gold key"), R::CoalesceKey(TEXT("gold_received"), {}), FString(TEXT("gold")));
    TestEqual(TEXT("exp key"), R::CoalesceKey(TEXT("exp_received"), {}), FString(TEXT("exp")));
    TestTrue(TEXT("quest never merges"), R::CoalesceKey(TEXT("quest_turned_in"), {}).IsEmpty());
    TestTrue(TEXT("skill never merges"), R::CoalesceKey(TEXT("skill_learned"), {}).IsEmpty());

    // Amount merge is additive.
    TestEqual(TEXT("3+2=5"), R::MergeAmounts(TEXT("3"), TEXT("2")), 5);
    TestEqual(TEXT("non-numeric is zero"), R::MergeAmounts(TEXT("x"), TEXT("2")), 2);
    TestEqual(TEXT("quantity field"), R::AmountField(TEXT("item_received")), FString(TEXT("quantity")));
    TestEqual(TEXT("gold field"), R::AmountField(TEXT("gold_received")), FString(TEXT("amount")));
    TestTrue(TEXT("quest has no amount field"), R::AmountField(TEXT("quest_turned_in")).IsEmpty());
    return true;
}
