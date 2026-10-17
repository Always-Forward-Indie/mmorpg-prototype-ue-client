#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// Contract specs for the NPC parser domain (roadmap P0.1).
// The domain was already TryGet-clean — this slice pins the contract and
// adds the FString-entry error rule (garbage/missing body). All functions
// under test are production JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCDataContractTest, "MMO.Protocol.NPCDataContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNPCDataContractTest::RunTest(const FString& Parameters)
{
    FNPCStruct Valid = JSONParser::DeserializeNPCData(ParserContract_ParseObj(
        TEXT("{\"id\":3,\"name\":\"Edrik\",\"slug\":\"edrik\",\"race\":\"human\",\"level\":10,"
        "\"npcType\":\"trainer\",\"isInteractable\":true,\"dialogueId\":\"edrik_intro\",\"questId\":\"\","
        "\"position\":{\"x\":500,\"y\":600,\"z\":50,\"rotationZ\":0},"
        "\"attributes\":[{\"id\":1,\"name\":\"Strength\",\"slug\":\"str\",\"value\":8},\"oops\"],"
        "\"quests\":[{\"slug\":\"first_hunt\",\"status\":\"available\"}],"
        "\"stats\":{\"health\":{\"current\":200,\"max\":200},\"mana\":{\"current\":50,\"max\":50}}}")));
    TestEqual(TEXT("valid id"), Valid.id, 3);
    TestEqual(TEXT("valid name"), Valid.name, FString(TEXT("Edrik")));
    TestEqual(TEXT("valid type"), Valid.npcType, FString(TEXT("trainer")));
    TestTrue(TEXT("valid interactable"), Valid.isInteractable);
    TestEqual(TEXT("valid dialogue"), Valid.dialogueId, FString(TEXT("edrik_intro")));
    TestEqual(TEXT("valid pos"), Valid.position.positionX, 500.0);
    TestEqual(TEXT("one attribute"), Valid.attributes.Num(), 1);
    TestEqual(TEXT("one quest"), Valid.quests.Num(), 1);
    if (Valid.quests.Num() == 1)
    {
        TestEqual(TEXT("quest slug"), Valid.quests[0].slug, FString(TEXT("first_hunt")));
        TestEqual(TEXT("quest status"), Valid.quests[0].status, FString(TEXT("available")));
    }
    TestEqual(TEXT("valid hp"), Valid.stats.health.current, 200);
    TestEqual(TEXT("valid mana max"), Valid.stats.mana.max, 50);

    // Truncated: level/isInteractable keep struct defaults (1/true), no crash.
    FNPCStruct Short = JSONParser::DeserializeNPCData(ParserContract_ParseObj(TEXT("{\"id\":4}")));
    TestEqual(TEXT("short level default"), Short.level, 1);
    TestTrue(TEXT("short interact default"), Short.isInteractable);

    // Null: defaults, no crash.
    FNPCStruct Null = JSONParser::DeserializeNPCData(nullptr);
    TestEqual(TEXT("null id default"), Null.id, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCSpawnDataContractTest, "MMO.Protocol.NPCSpawnDataContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FNPCSpawnDataContractTest::RunTest(const FString& Parameters)
{
    FNPCSpawnDataStruct Valid = JSONParser::DeserializeNPCSpawnData(
        TEXT("{\"header\":{},\"body\":{\"npcCount\":2,\"spawnRadius\":500.0,"
        "\"npcsSpawn\":[{\"id\":3,\"name\":\"Edrik\"},\"oops\",{\"id\":5,\"name\":\"Varan\"}]}}"));
    TestEqual(TEXT("valid count"), Valid.npcCount, 2);
    TestEqual(TEXT("valid radius"), Valid.spawnRadius, 500.0f);
    // Wrong-typed entry skipped quietly (no LogJson Error, no phantom NPC).
    TestEqual(TEXT("two npcs"), Valid.npcsSpawn.Num(), 2);
    if (Valid.npcsSpawn.Num() == 2)
    {
        TestEqual(TEXT("first name"), Valid.npcsSpawn[0].name, FString(TEXT("Edrik")));
        TestEqual(TEXT("second name"), Valid.npcsSpawn[1].name, FString(TEXT("Varan")));
    }

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeNPCSpawnData failed to parse JSON"));
    TestEqual(TEXT("garbage count default"), JSONParser::DeserializeNPCSpawnData(TEXT("not json")).npcCount, 0);

    // Missing body: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeNPCSpawnData missing body"));
    TestEqual(TEXT("nobody count default"), JSONParser::DeserializeNPCSpawnData(TEXT("{\"header\":{}}")).npcCount, 0);

    // Null body object: default, no crash.
    FNPCSpawnDataStruct Null = JSONParser::DeserializeNPCSpawnData(nullptr);
    TestEqual(TEXT("null count default"), Null.npcCount, 0);
    return true;
}
