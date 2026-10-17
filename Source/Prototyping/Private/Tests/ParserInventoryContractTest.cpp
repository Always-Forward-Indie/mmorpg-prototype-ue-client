#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// Contract specs for the Inventory parser domain (roadmap P0.1).
// Same rule as the other domains: fields parse quietly at struct defaults
// (explicit 0 durability still means broken — see DurabilityZeroKept);
// FString entries log one error-level line on garbage/missing body.
// All functions under test are production JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterInventoryContractTest, "MMO.Protocol.CharacterInventoryContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCharacterInventoryContractTest::RunTest(const FString& Parameters)
{
    FCharacterInventoryStruct Valid = JSONParser::DeserializeCharacterInventory(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"gold\":150,"
        "\"items\":["
        "{\"id\":101,\"itemId\":5,\"quantity\":1,\"durabilityCurrent\":80,\"durabilityMax\":100,\"isDurable\":true,"
        "\"item\":{\"slug\":\"sword_iron\",\"name\":\"Iron Sword\"}},"
        "\"oops\","
        "{\"id\":102,\"itemId\":6,\"quantity\":3}"
        "]}}"));
    TestEqual(TEXT("valid char"), Valid.characterId, 9001);
    TestEqual(TEXT("valid gold"), Valid.gold, 150);
    // Wrong-typed item entry skipped quietly (no LogJson Error).
    TestEqual(TEXT("two items"), Valid.items.Num(), 2);
    if (Valid.items.Num() == 2)
    {
        TestEqual(TEXT("first record"), Valid.items[0].id, 101);
        TestEqual(TEXT("first slug"), Valid.items[0].slug, FString(TEXT("sword_iron")));
        TestEqual(TEXT("first dura"), Valid.items[0].durabilityCurrent, 80);
        TestEqual(TEXT("second template"), Valid.items[1].itemId, 6);
        TestEqual(TEXT("second qty"), Valid.items[1].quantity, 3);
    }

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeCharacterInventory failed to parse JSON"));
    TestEqual(TEXT("garbage char default"), JSONParser::DeserializeCharacterInventory(TEXT("not json")).characterId, 0);

    // Missing body: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeCharacterInventory missing body"));
    TestEqual(TEXT("nobody gold default"), JSONParser::DeserializeCharacterInventory(TEXT("{\"header\":{}}")).gold, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryUpdateContractTest, "MMO.Protocol.InventoryUpdateContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryUpdateContractTest::RunTest(const FString& Parameters)
{
    // Packet convention: header/body wrap like every other entry.
    FInventoryUpdateStruct Valid = JSONParser::DeserializeInventoryUpdate(
        TEXT("{\"header\":{\"eventType\":\"INVENTORY_UPDATE\",\"status\":\"success\",\"message\":\"ok\"},"
        "\"body\":{\"eventType\":\"INVENTORY_UPDATE\",\"characterId\":9001,"
        "\"data\":{\"characterId\":9001,\"gold\":155,"
        "\"items\":[{\"id\":101,\"itemId\":5,\"quantity\":2}]}}}"));
    TestEqual(TEXT("valid event"), Valid.eventType, FString(TEXT("INVENTORY_UPDATE")));
    TestEqual(TEXT("valid char"), Valid.characterId, 9001);
    TestEqual(TEXT("valid gold"), Valid.data.gold, 155);
    TestEqual(TEXT("valid items"), Valid.data.items.Num(), 1);
    if (Valid.data.items.Num() == 1)
    {
        TestEqual(TEXT("item qty"), Valid.data.items[0].quantity, 2);
    }

    // Object-level partial: missing data stays empty, no crash.
    FInventoryUpdateStruct Partial = JSONParser::DeserializeInventoryUpdate(
        ParserContract_ParseObj(TEXT("{\"eventType\":\"INVENTORY_UPDATE\"}")));
    TestEqual(TEXT("partial event"), Partial.eventType, FString(TEXT("INVENTORY_UPDATE")));
    TestEqual(TEXT("partial items empty"), Partial.data.items.Num(), 0);

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeInventoryUpdate failed to parse JSON"));
    TestTrue(TEXT("garbage event empty"), JSONParser::DeserializeInventoryUpdate(TEXT("not json")).eventType.IsEmpty());

    // Missing body: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeInventoryUpdate missing body"));
    TestTrue(TEXT("nobody event empty"), JSONParser::DeserializeInventoryUpdate(TEXT("{\"header\":{}}")).eventType.IsEmpty());

    // Null object: default, no crash.
    FInventoryUpdateStruct Null = JSONParser::DeserializeInventoryUpdate(nullptr);
    TestTrue(TEXT("null event empty"), Null.eventType.IsEmpty());
    return true;
}
