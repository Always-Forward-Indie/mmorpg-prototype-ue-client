#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Data/WIODataStructs.h"
#include "Authentication/LoginFlowTypes.h"
#include "ParserContractHelpers.h"

// Contract specs for the combat parser domain (roadmap P0.1).
// Wrong-typed ARRAY entries skip quietly via TryGetObject — the old
// AsObject() logged a LogJson Error per entry AND yielded a phantom
// zeroed struct (AsObject returns a valid empty object on type mismatch).
// Leaf fields parse quietly at struct defaults; FString entries log one
// error-level line on garbage/missing body.

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillInitiationContractTest, "MMO.Protocol.SkillInitiationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillInitiationContractTest::RunTest(const FString& Parameters)
{
    FSkillInitiationData Valid = JSONParser::DeserializeSkillInitiation(
        TEXT("{\"header\":{\"eventType\":\"combatInitiation\",\"status\":\"success\",\"message\":\"ok\"},"
        "\"body\":{\"skillInitiation\":{\"skillName\":\"Fireball\",\"skillSlug\":\"fireball\","
        "\"animationName\":\"cast_fire\",\"animationDuration\":1.2,\"castTime\":0.5,"
        "\"casterId\":7,\"targetId\":101,\"success\":true,"
        "\"skillEffectType\":\"damage\",\"skillSchool\":\"fire\","
        "\"cooldownMs\":3000,\"gcdMs\":1500,\"serverTimestamp\":123456789}}}"));
    TestTrue(TEXT("valid success"), Valid.success);
    TestEqual(TEXT("valid slug"), Valid.skillSlug, FString(TEXT("fireball")));
    TestEqual(TEXT("valid caster"), Valid.casterId, 7);
    TestEqual(TEXT("valid target"), Valid.targetId, 101);
    TestEqual(TEXT("valid effect"), Valid.skillEffectType, ESkillEffectType::Damage);
    TestEqual(TEXT("valid school"), Valid.skillSchool, ESkillSchool::Fire);
    TestEqual(TEXT("valid cooldown"), Valid.cooldownMs, 3000);
    TestEqual(TEXT("valid server ts"), Valid.serverTimestamp, static_cast<int64>(123456789));

    // Unknown enum strings map to None, no crash.
    FSkillInitiationData UnknownEnum = JSONParser::DeserializeSkillInitiation(
        TEXT("{\"header\":{},\"body\":{\"skillInitiation\":{\"skillEffectType\":\"chaos\",\"skillSchool\":\"void\"}}}"));
    TestEqual(TEXT("unknown effect none"), UnknownEnum.skillEffectType, ESkillEffectType::None);
    TestEqual(TEXT("unknown school none"), UnknownEnum.skillSchool, ESkillSchool::None);

    // Garbage: failed + errorCode empty + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeSkillInitiation failed to parse JSON"));
    FSkillInitiationData Garbage = JSONParser::DeserializeSkillInitiation(TEXT("not json"));
    TestFalse(TEXT("garbage fails"), Garbage.success);

    // Missing body: failed + errorCode empty + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeSkillInitiation missing body"));
    FSkillInitiationData NoBody = JSONParser::DeserializeSkillInitiation(TEXT("{\"header\":{}}"));
    TestFalse(TEXT("nobody fails"), NoBody.success);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillResultFieldsContractTest, "MMO.Protocol.SkillResultFieldsContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillResultFieldsContractTest::RunTest(const FString& Parameters)
{
    // Server heal shape + header-wrapped success.
    FSkillResultData Heal = JSONParser::DeserializeSkillResult(
        TEXT("{\"header\":{\"eventType\":\"healingResult\",\"status\":\"success\",\"message\":\"ok\"},"
        "\"body\":{\"skillResult\":{\"skillSlug\":\"mend\",\"casterId\":7,\"targetId\":7,"
        "\"healing\":40,\"manaHealing\":10,\"finalTargetHealth\":120,\"success\":true}}}"));
    TestEqual(TEXT("heal amount"), Heal.healing, 40);
    TestEqual(TEXT("mana heal"), Heal.manaHealing, 10);
    TestTrue(TEXT("heal success"), Heal.success);

    // Garbage: failed + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeSkillResult failed to parse JSON"));
    FSkillResultData Garbage = JSONParser::DeserializeSkillResult(TEXT("not json"));
    TestFalse(TEXT("garbage fails"), Garbage.success);

    // Missing body: failed, keeps header code + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeSkillResult missing body"));
    FSkillResultData NoBody = JSONParser::DeserializeSkillResult(
        TEXT("{\"header\":{\"eventType\":\"combatResult\",\"status\":\"error\",\"message\":\"OUT_OF_RANGE\"},\"bodyx\":{}}"));
    TestFalse(TEXT("nobody fails"), NoBody.success);
    TestEqual(TEXT("nobody keeps code"), NoBody.errorCode, FString(TEXT("OUT_OF_RANGE")));
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
