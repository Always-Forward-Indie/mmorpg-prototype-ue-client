#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// Contract specs for stats_update (via its single owner PlayerAttributeParser)
// and the experience-gain event (roadmap P0.1 remainder).
// Stats validation logs Error on invalid data (bad character/health/mana) —
// that is the seam check, pinned below with AddExpectedErrorPlain.
// All functions under test are production code (JSONParser entry delegating
// to PlayerAttributeParser — no local re-implementation).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsUpdateContractTest, "MMO.Protocol.StatsUpdateContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FStatsUpdateContractTest::RunTest(const FString& Parameters)
{
    FPlayerStatsUpdateStruct Valid = JSONParser::DeserializePlayerStatsUpdate(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"level\":5,\"freeSkillPoints\":2,"
        "\"health\":{\"current\":80,\"max\":100},\"mana\":{\"current\":10,\"max\":50},"
        "\"experience\":{\"current\":100,\"levelStart\":0,\"nextLevel\":1500,\"debt\":0},"
        "\"attributes\":[{\"slug\":\"str\",\"name\":\"Strength\",\"base\":10,\"effective\":12},\"oops\"],"
        "\"activeEffects\":[{\"slug\":\"well_fed\",\"value\":5}],"
        "\"weight\":{\"current\":12.5,\"max\":100}}}"));
    TestEqual(TEXT("valid char"), Valid.characterId, 9001);
    TestEqual(TEXT("valid level"), Valid.level, 5);
    TestEqual(TEXT("valid hp"), Valid.healthCurrent, 80);
    TestEqual(TEXT("valid max"), Valid.healthMax, 100);
    TestEqual(TEXT("valid mana"), Valid.manaCurrent, 10);
    TestEqual(TEXT("valid exp"), Valid.experienceCurrent, 100);
    TestEqual(TEXT("valid debt"), Valid.experienceDebt, 0);
    TestEqual(TEXT("one attribute"), Valid.attributes.Num(), 1);
    if (Valid.attributes.Num() == 1)
    {
        TestEqual(TEXT("attr slug"), Valid.attributes[0].slug, FString(TEXT("str")));
        TestEqual(TEXT("attr base"), Valid.attributes[0].base, 10.0f);
        TestEqual(TEXT("attr eff"), Valid.attributes[0].effective, 12.0f);
    }
    TestEqual(TEXT("one effect"), Valid.activeEffects.Num(), 1);
    TestEqual(TEXT("weight max"), Valid.weightMax, 100.0f);

    // Garbage: default + the parser's own error log.
    AddExpectedErrorPlain(TEXT("PlayerAttributeParser: Failed to parse JSON string"));
    TestEqual(TEXT("garbage char default"), JSONParser::DeserializePlayerStatsUpdate(TEXT("not json")).characterId, 0);

    // Missing body: default + the parser's own error log.
    AddExpectedErrorPlain(TEXT("PlayerAttributeParser: No valid body found in JSON"));
    TestEqual(TEXT("nobody char default"), JSONParser::DeserializePlayerStatsUpdate(TEXT("{\"header\":{}}")).characterId, 0);

    // Invalid data (no character): default-shaped + validation error pinned.
    // NOTE: validation fires once per malformed update by design (seam check).
    AddExpectedErrorPlain(TEXT("PlayerAttributeParser: Invalid character ID"));
    FPlayerStatsUpdateStruct NoChar = JSONParser::DeserializePlayerStatsUpdate(TEXT("{\"header\":{},\"body\":{}}"));
    TestEqual(TEXT("nochar id default"), NoChar.characterId, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGainEventContractTest, "MMO.Protocol.GainEventContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FGainEventContractTest::RunTest(const FString& Parameters)
{
    FExperienceGainEventStruct Valid = JSONParser::DeserializeExperienceGainEvent(
        TEXT("{\"header\":{},\"body\":{\"experienceEvent\":{\"experienceGained\":100,"
        "\"reason\":\"mob_kill\",\"sourceId\":4,\"sourceName\":\"Forest Fox\"}}}"));
    TestEqual(TEXT("valid gained"), Valid.experienceGained, 100);
    TestEqual(TEXT("valid reason text"), Valid.reasonText, FString(TEXT("mob_kill")));
    TestEqual(TEXT("valid reason enum"), Valid.reason, EExperienceReason::MobKill);
    TestEqual(TEXT("valid source"), Valid.sourceName, FString(TEXT("Forest Fox")));

    // Flat body without the wrapper object parses directly (fallback).
    FExperienceGainEventStruct Flat = JSONParser::DeserializeExperienceGainEvent(
        TEXT("{\"header\":{},\"body\":{\"experienceGained\":25,\"reason\":\"quest_complete\"}}"));
    TestEqual(TEXT("flat gained"), Flat.experienceGained, 25);
    TestEqual(TEXT("flat reason"), Flat.reason, EExperienceReason::QuestComplete);

    // Unknown reason maps to None, no crash.
    FExperienceGainEventStruct Unknown = JSONParser::DeserializeExperienceGainEvent(
        TEXT("{\"header\":{},\"body\":{\"experienceEvent\":{\"reason\":\"lottery\"}}}"));
    TestEqual(TEXT("unknown reason none"), Unknown.reason, EExperienceReason::None);

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeExperienceGainEvent failed to parse JSON"));
    TestEqual(TEXT("garbage gained default"), JSONParser::DeserializeExperienceGainEvent(TEXT("not json")).experienceGained, 0);

    // Missing body: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeExperienceGainEvent missing body"));
    TestEqual(TEXT("nobody gained default"), JSONParser::DeserializeExperienceGainEvent(TEXT("{\"header\":{}}")).experienceGained, 0);
    return true;
}
