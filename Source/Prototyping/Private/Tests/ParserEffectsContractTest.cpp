#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// Contract specs for the effect-stream entries (roadmap P0.1).
// Hot-path exception (same as NetworkHeader): effect ticks arrive per DoT
// tick, so malformed input yields quiet defaults — a per-tick Error would
// spam the log on any hiccup. Dropped ticks surface observably (no heal/
// damage numbers, mob HP divergence). All functions under test are
// production JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectTickContractTest, "MMO.Protocol.EffectTickContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FEffectTickContractTest::RunTest(const FString& Parameters)
{
    FEffectTickData Valid = JSONParser::DeserializeEffectTick(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"effectSlug\":\"burn\","
        "\"effectTypeSlug\":\"damage\",\"value\":12,\"isHeal\":false,"
        "\"newHealth\":68,\"newMana\":10,\"targetDied\":false}}"));
    TestEqual(TEXT("valid char"), Valid.characterId, 9001);
    TestEqual(TEXT("valid slug"), Valid.effectSlug, FString(TEXT("burn")));
    TestEqual(TEXT("valid value"), Valid.value, 12);
    TestFalse(TEXT("valid not heal"), Valid.bIsHeal);
    TestEqual(TEXT("valid hp"), Valid.newHealth, 68);
    TestFalse(TEXT("valid alive"), Valid.targetDied);

    // Truncated: defaults, no crash.
    FEffectTickData Short = JSONParser::DeserializeEffectTick(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001}}"));
    TestEqual(TEXT("short value default"), Short.value, 0);

    // Garbage and header-only: quiet defaults (hot path, no log).
    FEffectTickData Garbage = JSONParser::DeserializeEffectTick(TEXT("not json"));
    TestEqual(TEXT("garbage char default"), Garbage.characterId, 0);
    FEffectTickData NoBody = JSONParser::DeserializeEffectTick(TEXT("{\"header\":{}}"));
    TestEqual(TEXT("nobody char default"), NoBody.characterId, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActiveEffectsContractTest, "MMO.Protocol.ActiveEffectsContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FActiveEffectsContractTest::RunTest(const FString& Parameters)
{
    TArray<FActiveEffectEntry> Valid = JSONParser::DeserializeActiveEffectsPacket(
        TEXT("{\"header\":{},\"body\":{\"activeEffects\":["
        "{\"slug\":\"battle_cry_phys_atk\",\"effectType\":\"buff\",\"effectTypeSlug\":\"buff\","
        "\"attributeSlug\":\"phys_atk\",\"sourceType\":\"skill\",\"value\":15,\"expiresAt\":123999,"
        "\"isPercentage\":false,\"isPermanent\":false},"
        "\"oops\","
        "{\"slug\":\"well_fed\",\"isPermanent\":true}"
        "]}}"));
    // Wrong-typed entry skipped quietly (no LogJson Error, no phantom).
    TestEqual(TEXT("two effects"), Valid.Num(), 2);
    if (Valid.Num() == 2)
    {
        TestEqual(TEXT("first slug"), Valid[0].slug, FString(TEXT("battle_cry_phys_atk")));
        TestEqual(TEXT("first value"), Valid[0].value, 15.0f);
        TestEqual(TEXT("first expiry"), Valid[0].expiresAt, static_cast<int64>(123999));
        TestEqual(TEXT("second slug"), Valid[1].slug, FString(TEXT("well_fed")));
        TestTrue(TEXT("second permanent"), Valid[1].bIsPermanent);
    }

    // Garbage and header-only: quiet empty (hot path, no log).
    TestEqual(TEXT("garbage empty"), JSONParser::DeserializeActiveEffectsPacket(TEXT("not json")).Num(), 0);
    TestEqual(TEXT("nobody empty"), JSONParser::DeserializeActiveEffectsPacket(TEXT("{\"header\":{}}")).Num(), 0);
    return true;
}
