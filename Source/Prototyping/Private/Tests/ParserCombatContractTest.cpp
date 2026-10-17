#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// Contract specs for the list-loop hardening sweep (roadmap P0.1).
// Wrong-typed ARRAY entries skip quietly via TryGetObject — the old
// AsObject() logged a LogJson Error per entry AND yielded a phantom
// zeroed struct (AsObject returns a valid empty object on type mismatch).
// Leaf-field hardening of the combat domain lands in its own slice;
// object entries below use correct types on purpose.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAppliedEffectsFormatsTest, "MMO.Protocol.AppliedEffectsFormats",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FAppliedEffectsFormatsTest::RunTest(const FString& Parameters)
{
    TArray<TSharedPtr<FJsonValue>> Arr = ParserContract_ParseArrayField(
        TEXT("{\"appliedEffects\":["
        "{\"effectName\":\"burn\",\"duration\":5,\"value\":10,\"effectType\":\"damage\"},"
        "\"battle_cry_phys_atk\","
        "42"
        "]}"), TEXT("appliedEffects"));
    TArray<FAppliedEffectData> Effects = JSONParser::DeserializeAppliedEffects(Arr);
    // Object + slug-string kept, number skipped quietly (no LogJson Error).
    TestEqual(TEXT("two effects"), Effects.Num(), 2);
    if (Effects.Num() == 2)
    {
        TestEqual(TEXT("object name"), Effects[0].effectName, FString(TEXT("burn")));
        TestEqual(TEXT("object value"), Effects[0].value, 10);
        TestEqual(TEXT("slug name"), Effects[1].effectName, FString(TEXT("battle_cry_phys_atk")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillResultAoeSkipTest, "MMO.Protocol.SkillResultAoeSkip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillResultAoeSkipTest::RunTest(const FString& Parameters)
{
    FSkillResultData Batch = JSONParser::DeserializeSkillResult(
        TEXT("{\"header\":{\"eventType\":\"combatAoeResult\",\"status\":\"success\",\"message\":\"ok\"},\"body\":{"
        "\"skillSlug\":\"fireball\",\"casterId\":7,\"targetId\":101,"
        "\"damage\":50,\"success\":true,"
        "\"aoeResult\":{\"targets\":["
        "{\"targetId\":101,\"damage\":50,\"finalTargetHealth\":30},"
        "7,"
        "{\"targetId\":102,\"damage\":43,\"isCritical\":true,\"finalTargetHealth\":12}"
        "]}}}"));
    // Wrong-typed target skipped quietly — no phantom zero-damage entry.
    TestEqual(TEXT("two targets"), Batch.aoeTargets.Num(), 2);
    if (Batch.aoeTargets.Num() == 2)
    {
        TestEqual(TEXT("first id"), Batch.aoeTargets[0].targetId, 101);
        TestEqual(TEXT("second id"), Batch.aoeTargets[1].targetId, 102);
        TestTrue(TEXT("second crit"), Batch.aoeTargets[1].isCritical);
    }
    return true;
}
