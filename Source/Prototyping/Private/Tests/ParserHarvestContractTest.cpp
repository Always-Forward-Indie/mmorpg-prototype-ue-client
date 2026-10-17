#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// Contract specs for the Harvest parser domain (roadmap P0.1).
// Same rule as Core/Mobs: fields parse quietly at struct defaults;
// FString entries log one error-level line on garbage/missing body.
// All functions under test are production JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarvestStartedContractTest, "MMO.Protocol.HarvestStartedContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHarvestStartedContractTest::RunTest(const FString& Parameters)
{
    FHarvestStartedStruct Valid = JSONParser::DeserializeHarvestStarted(
        TEXT("{\"header\":{},\"body\":{\"type\":\"harvest_started\",\"clientId\":11,\"playerId\":9001,"
        "\"corpseId\":501,\"duration\":10,\"startTime\":123456789}}"));
    TestEqual(TEXT("valid type"), Valid.type, FString(TEXT("harvest_started")));
    TestEqual(TEXT("valid corpse"), Valid.corpseId, 501);
    TestEqual(TEXT("valid duration"), Valid.duration, 10);
    TestEqual(TEXT("valid start"), Valid.startTime, static_cast<int64>(123456789));

    // Truncated: defaults, no crash.
    FHarvestStartedStruct Short = JSONParser::DeserializeHarvestStarted(
        TEXT("{\"header\":{},\"body\":{\"corpseId\":502}}"));
    TestEqual(TEXT("short corpse"), Short.corpseId, 502);
    TestEqual(TEXT("short duration default"), Short.duration, 0);

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeHarvestStarted failed to parse JSON"));
    TestEqual(TEXT("garbage corpse default"), JSONParser::DeserializeHarvestStarted(TEXT("not json")).corpseId, 0);

    // Missing body: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeHarvestStarted missing body"));
    TestEqual(TEXT("nobody corpse default"), JSONParser::DeserializeHarvestStarted(TEXT("{\"header\":{}}")).corpseId, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarvestCompleteContractTest, "MMO.Protocol.HarvestCompleteContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHarvestCompleteContractTest::RunTest(const FString& Parameters)
{
    FHarvestCompleteStruct Valid = JSONParser::DeserializeHarvestComplete(
        TEXT("{\"header\":{},\"body\":{\"type\":\"harvest_complete\",\"clientId\":11,\"playerId\":9001,"
        "\"corpseId\":501,\"success\":true,\"totalItems\":2,"
        "\"availableLoot\":["
        "{\"itemId\":46,\"itemSlug\":\"wolf_fang\",\"quantity\":2,\"name\":\"Wolf Fang\",\"rarityId\":2,\"rarityName\":\"Uncommon\",\"weight\":0.1,\"isHarvestItem\":true},"
        "7,"
        "{\"itemId\":47,\"quantity\":1}"
        "]}}"));
    TestTrue(TEXT("valid success"), Valid.success);
    TestEqual(TEXT("valid total"), Valid.totalItems, 2);
    // Wrong-typed loot entry skipped quietly (no LogJson Error, no phantom).
    TestEqual(TEXT("two loot items"), Valid.availableLoot.Num(), 2);
    if (Valid.availableLoot.Num() == 2)
    {
        TestEqual(TEXT("first slug"), Valid.availableLoot[0].itemSlug, FString(TEXT("wolf_fang")));
        TestEqual(TEXT("first qty"), Valid.availableLoot[0].quantity, 2);
        TestEqual(TEXT("first rarity default kept"), Valid.availableLoot[0].rarityId, 2);
        TestEqual(TEXT("second id"), Valid.availableLoot[1].itemId, 47);
        TestEqual(TEXT("second rarity default"), Valid.availableLoot[1].rarityId, 1);
        TestEqual(TEXT("second rarity name default"), Valid.availableLoot[1].rarityName, FString(TEXT("Common")));
    }

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeHarvestComplete failed to parse JSON"));
    TestFalse(TEXT("garbage not success"), JSONParser::DeserializeHarvestComplete(TEXT("not json")).success);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarvestErrorContractTest, "MMO.Protocol.HarvestErrorContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHarvestErrorContractTest::RunTest(const FString& Parameters)
{
    FHarvestErrorStruct Err = JSONParser::DeserializeHarvestError(
        TEXT("{\"header\":{},\"body\":{\"type\":\"harvest_error\",\"corpseId\":501,"
        "\"errorCode\":\"CORPSE_EMPTY\",\"message\":\"Nothing left\"}}"));
    TestEqual(TEXT("error code"), Err.errorCode, FString(TEXT("CORPSE_EMPTY")));
    TestEqual(TEXT("error corpse"), Err.corpseId, 501);

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeHarvestError failed to parse JSON"));
    TestTrue(TEXT("garbage code empty"), JSONParser::DeserializeHarvestError(TEXT("not json")).errorCode.IsEmpty());

    FCorpseLootPickupResponseStruct Pick = JSONParser::DeserializeCorpseLootPickupResponse(
        TEXT("{\"header\":{},\"body\":{\"success\":true,\"corpseUID\":501,\"itemsPickedUp\":1,"
        "\"pickedUpItems\":[{\"itemId\":46,\"quantity\":2}],"
        "\"remainingLoot\":[]}}"));
    TestTrue(TEXT("pickup success"), Pick.success);
    TestEqual(TEXT("pickup count"), Pick.itemsPickedUp, 1);
    TestEqual(TEXT("pickup items"), Pick.pickedUpItems.Num(), 1);
    TestEqual(TEXT("remaining empty"), Pick.remainingLoot.Num(), 0);

    FCorpseLootPickupErrorStruct PickErr = JSONParser::DeserializeCorpseLootPickupError(
        TEXT("{\"header\":{},\"body\":{\"success\":false,\"errorCode\":\"TOO_FAR\",\"corpseUID\":501}}"));
    TestEqual(TEXT("pickerr code"), PickErr.errorCode, FString(TEXT("TOO_FAR")));
    return true;
}
