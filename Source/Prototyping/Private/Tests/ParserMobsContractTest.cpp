#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Data/WIODataStructs.h"
#include "Authentication/LoginFlowTypes.h"
#include "ParserContractHelpers.h"

// Contract specs for the Mobs parser domain (roadmap P0.1).
// Same rule as Core: truncated/wrong-typed packets parse quietly at struct
// defaults (no LogJson spam, no crash); FString entries log one error-level
// line on garbage/missing body. All functions under test are production
// JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobDataContractTest, "MMO.Protocol.MobDataContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobDataContractTest::RunTest(const FString& Parameters)
{
    FMOBStruct Valid = JSONParser::DeserializeMobData(ParserContract_ParseObj(
        TEXT("{\"id\":4,\"uid\":501,\"zoneId\":9,\"name\":\"Forest Fox\",\"slug\":\"forest_fox\",\"race\":\"beast\",\"level\":3,"
        "\"isAggressive\":true,\"isDead\":false,\"combatState\":2,"
        "\"position\":{\"x\":100,\"y\":200,\"z\":50,\"rotationZ\":180},"
        "\"stats\":{\"health\":{\"current\":80,\"max\":100},\"mana\":{\"current\":10}},"
        "\"attributes\":[{\"id\":1,\"name\":\"Strength\",\"slug\":\"str\",\"value\":6}],"
        "\"velocity\":{\"dirX\":0.6,\"dirY\":0.8,\"speed\":300},"
        "\"unknownFuture\":\"ignored\"}")));
    TestEqual(TEXT("valid id"), Valid.mobID, 4);
    TestEqual(TEXT("valid uid"), Valid.mobUniqueID, FString(TEXT("501")));
    TestEqual(TEXT("valid zone"), Valid.mobZoneID, 9);
    TestEqual(TEXT("valid name"), Valid.mobName, FString(TEXT("Forest Fox")));
    TestEqual(TEXT("valid slug"), Valid.mobSlug, FString(TEXT("forest_fox")));
    TestEqual(TEXT("valid level"), Valid.mobLevel, 3);
    TestTrue(TEXT("valid aggressive"), Valid.bIsAggressive);
    TestFalse(TEXT("valid alive"), Valid.bIsDead);
    TestEqual(TEXT("valid combat"), Valid.mobCombatState, 2);
    TestEqual(TEXT("valid pos"), Valid.mobPosition.positionX, 100.0);
    TestEqual(TEXT("valid hp"), Valid.mobCurrentHealth, 80);
    TestEqual(TEXT("valid mana"), Valid.mobCurrentMana, 10);
    TestTrue(TEXT("valid attr"), Valid.mobAttributes.attributesData.Contains(TEXT("str")));
    TestEqual(TEXT("valid speed"), Valid.mobVelocity.speed, 300.0f);

    // Truncated: missing stats/position/velocity stay default, no crash.
    FMOBStruct Short = JSONParser::DeserializeMobData(ParserContract_ParseObj(
        TEXT("{\"id\":5,\"uid\":502,\"name\":\"Cub\"}")));
    TestEqual(TEXT("short id"), Short.mobID, 5);
    TestEqual(TEXT("short hp default"), Short.mobCurrentHealth, 0);
    TestEqual(TEXT("short pos default"), Short.mobPosition.positionX, 0.0);
    TestFalse(TEXT("short dead default"), Short.bIsDead);

    // Stats without health object: no crash (used to null-deref), zeros kept.
    FMOBStruct NoHealth = JSONParser::DeserializeMobData(ParserContract_ParseObj(
        TEXT("{\"id\":6,\"uid\":503,\"stats\":{\"mana\":{\"current\":7}}}")));
    TestEqual(TEXT("nohealth hp default"), NoHealth.mobCurrentHealth, 0);
    TestEqual(TEXT("nohealth mana kept"), NoHealth.mobCurrentMana, 7);

    // Null object: struct defaults, no crash.
    FMOBStruct Null = JSONParser::DeserializeMobData(nullptr);
    TestEqual(TEXT("null id default"), Null.mobID, 0);
    TestTrue(TEXT("null uid default empty"), Null.mobUniqueID.IsEmpty());

    // Object without uid keeps the legacy effective default ("0"), quietly.
    FMOBStruct NoUid = JSONParser::DeserializeMobData(ParserContract_ParseObj(TEXT("{\"id\":8}")));
    TestTrue(TEXT("nouid legacy zero string"), NoUid.mobUniqueID == FString(TEXT("0")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobMoveUpdateContractTest, "MMO.Protocol.MobMoveUpdateContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobMoveUpdateContractTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> Body = ParserContract_ParseObj(
        TEXT("{\"mobs\":["
        "{\"uid\":501,\"combatState\":1,\"stepTimestampMs\":123456,"
        "\"position\":{\"x\":10,\"y\":20,\"z\":5,\"rotationZ\":90},"
        "\"velocity\":{\"dirX\":1,\"dirY\":0,\"speed\":250},"
        "\"waypoint\":{\"x\":30,\"y\":40}},"
        "{\"uid\":502,\"position\":{\"x\":1,\"y\":2}},"
        "42"
        "]}"));
    TArray<JSONParser::FMobMovePacketEntry> Entries = JSONParser::DeserializeMobMoveUpdate(Body, 123999);
    // The non-object entry is skipped quietly (TryGetObject, no LogJson Error).
    TestEqual(TEXT("two entries"), Entries.Num(), 2);
    if (Entries.Num() == 2)
    {
        TestEqual(TEXT("first uid"), Entries[0].uid, 501);
        TestEqual(TEXT("first combat"), Entries[0].moveEntry.combatState, 1);
        TestEqual(TEXT("first step"), Entries[0].moveEntry.stepTimestampMs, static_cast<int64>(123456));
        TestEqual(TEXT("first pos"), Entries[0].moveEntry.position.positionX, 10.0);
        TestEqual(TEXT("first speed"), Entries[0].moveEntry.speed, 250.0f);
        TestTrue(TEXT("first waypoint"), Entries[0].moveEntry.bHasWaypoint);
        TestEqual(TEXT("first wpx"), Entries[0].moveEntry.waypointX, 30.0f);
        // Truncated second entry: partial position kept, rest defaults.
        TestEqual(TEXT("second uid"), Entries[1].uid, 502);
        TestEqual(TEXT("second pos x"), Entries[1].moveEntry.position.positionX, 1.0);
        TestEqual(TEXT("second pos z default"), Entries[1].moveEntry.position.positionZ, 0.0);
        TestFalse(TEXT("second no waypoint"), Entries[1].moveEntry.bHasWaypoint);
    }

    // Missing mobs array: empty, no crash. Null body: empty.
    TestEqual(TEXT("no array empty"), JSONParser::DeserializeMobMoveUpdate(ParserContract_ParseObj(TEXT("{}")), 0).Num(), 0);
    TestEqual(TEXT("null empty"), JSONParser::DeserializeMobMoveUpdate(nullptr, 0).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobTargetLostContractTest, "MMO.Protocol.MobTargetLostContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobTargetLostContractTest::RunTest(const FString& Parameters)
{
    FMobTargetLostStruct Valid = JSONParser::DeserializeMobTargetLost(ParserContract_ParseObj(
        TEXT("{\"lostTargetPlayerId\":9001,\"mobId\":4,\"mobUID\":501,"
        "\"positionX\":10,\"positionY\":20,\"positionZ\":5,\"rotationZ\":90}")));
    TestEqual(TEXT("valid player"), Valid.lostTargetPlayerId, 9001);
    TestEqual(TEXT("valid mob"), Valid.mobId, 4);
    TestEqual(TEXT("valid uid"), Valid.mobUID, 501);
    TestEqual(TEXT("valid x"), Valid.position.positionX, 10.0);

    // Partial position: each field independent (old code zeroed everything
    // unless all four were present — pinned new behavior).
    FMobTargetLostStruct Partial = JSONParser::DeserializeMobTargetLost(ParserContract_ParseObj(
        TEXT("{\"mobUID\":502,\"positionX\":7}")));
    TestEqual(TEXT("partial uid"), Partial.mobUID, 502);
    TestEqual(TEXT("partial x kept"), Partial.position.positionX, 7.0);
    TestEqual(TEXT("partial y default"), Partial.position.positionY, 0.0);

    // Garbage string: default, silent (object entry — packet-level error is
    // logged by the FString entries that own a body contract, not here).
    FMobTargetLostStruct Garbage = JSONParser::DeserializeMobTargetLost(TEXT("not json"));
    TestEqual(TEXT("garbage uid default"), Garbage.mobUID, 0);

    // Null body: default, no crash.
    FMobTargetLostStruct Null = JSONParser::DeserializeMobTargetLost(nullptr);
    TestEqual(TEXT("null uid default"), Null.mobUID, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobsListContractTest, "MMO.Protocol.MobsListContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobsListContractTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> Body = ParserContract_ParseObj(
        TEXT("{\"mobs\":["
        "{\"id\":4,\"uid\":501,\"name\":\"Fox\",\"level\":3},"
        "\"oops\","
        "{\"id\":5,\"uid\":502,\"name\":\"Wolf\"}"
        "]}"));
    TArray<FMOBStruct> Flat = JSONParser::DeserializeMobsList(Body);
    // Wrong-typed entry skipped quietly (no LogJson Error, no phantom mob).
    TestEqual(TEXT("two mobs"), Flat.Num(), 2);
    if (Flat.Num() == 2)
    {
        TestEqual(TEXT("first uid"), Flat[0].mobUniqueID, FString(TEXT("501")));
        TestEqual(TEXT("second name"), Flat[1].mobName, FString(TEXT("Wolf")));
    }

    // Missing array and null body: empty, no crash.
    TestEqual(TEXT("no array empty"), JSONParser::DeserializeMobsList(ParserContract_ParseObj(TEXT("{}"))).Num(), 0);
    TestEqual(TEXT("null empty"), JSONParser::DeserializeMobsList(nullptr).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobRankContractTest, "MMO.Protocol.MobRankContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FMobRankContractTest::RunTest(const FString& Parameters)
{
    // S2.5: machine-readable champion markers ride the spawn entry.
    FMOBStruct Champ = JSONParser::DeserializeMobData(ParserContract_ParseObj(
        TEXT("{\"id\":6,\"uid\":4243,\"slug\":\"ForestFox\",\"level\":3,"
        "\"rankCode\":\"champion\",\"isChampion\":true,\"championKind\":\"threshold\","
        "\"stats\":{\"health\":{\"current\":210,\"max\":210}}}")));
    TestEqual(TEXT("rank kept"), Champ.mobRankCode, FString(TEXT("champion")));
    TestTrue(TEXT("champion flag"), Champ.bIsChampion);
    TestEqual(TEXT("kind kept"), Champ.championKind, FString(TEXT("threshold")));

    // Normal mobs: defaults, no crash.
    FMOBStruct Normal = JSONParser::DeserializeMobData(ParserContract_ParseObj(
        TEXT("{\"id\":6,\"uid\":4244,\"slug\":\"ForestFox\"}")));
    TestEqual(TEXT("default rank"), Normal.mobRankCode, FString(TEXT("normal")));
    TestFalse(TEXT("not champion"), Normal.bIsChampion);
    TestTrue(TEXT("kind empty"), Normal.championKind.IsEmpty());
    return true;
}
