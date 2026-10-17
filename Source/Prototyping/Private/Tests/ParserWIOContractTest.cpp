#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// Contract specs for the WIO parser domain (roadmap P0.1).
// The domain was already TryGet-clean — this slice pins the contract and
// adds the FString-entry error rule (garbage/missing body). All functions
// under test are production JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldObjectsListContractTest, "MMO.Protocol.WorldObjectsListContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FWorldObjectsListContractTest::RunTest(const FString& Parameters)
{
    TArray<FWorldObjectData> Valid = JSONParser::DeserializeWorldObjectsList(
        TEXT("{\"header\":{},\"body\":{\"worldObjects\":["
        "{\"id\":9001,\"slug\":\"examine_stone\",\"nameKey\":\"stone\",\"objectType\":\"examine\","
        "\"scope\":\"global\",\"posX\":100,\"posY\":200,\"posZ\":50,\"rotZ\":0,"
        "\"interactionRadius\":180,\"zoneId\":9,\"channelTimeSec\":0,\"respawnSec\":0,"
        "\"minLevel\":1,\"currentState\":\"active\",\"unknownFuture\":1},"
        "\"oops\","
        "{\"id\":9002,\"slug\":\"old_chest\",\"objectType\":\"search\",\"scope\":\"perplayer\","
        "\"currentState\":\"depleted\",\"channelTimeSec\":3}"
        "]}}"));
    // Wrong-typed entry skipped quietly (no LogJson Error, no phantom).
    TestEqual(TEXT("two objects"), Valid.Num(), 2);
    if (Valid.Num() == 2)
    {
        TestEqual(TEXT("first id"), Valid[0].ObjectId, 9001);
        TestEqual(TEXT("first slug"), Valid[0].Slug, FString(TEXT("examine_stone")));
        TestEqual(TEXT("first type"), Valid[0].ObjectType, EWIOObjectType::Examine);
        TestEqual(TEXT("first scope"), Valid[0].Scope, EWIOScope::Global);
        TestEqual(TEXT("first state"), Valid[0].CurrentState, EWIOState::Active);
        TestEqual(TEXT("first pos"), Valid[0].PosX, 100.0f);
        TestEqual(TEXT("second type"), Valid[1].ObjectType, EWIOObjectType::Search);
        TestEqual(TEXT("second state"), Valid[1].CurrentState, EWIOState::Depleted);
        TestEqual(TEXT("second channel"), Valid[1].ChannelTimeSec, 3);
    }

    // Garbage: empty + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeWorldObjectsList failed to parse JSON"));
    TestEqual(TEXT("garbage empty"), JSONParser::DeserializeWorldObjectsList(TEXT("not json")).Num(), 0);

    // Missing body: empty + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeWorldObjectsList missing body"));
    TestEqual(TEXT("nobody empty"), JSONParser::DeserializeWorldObjectsList(TEXT("{\"header\":{}}")).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWIOInteractionContractTest, "MMO.Protocol.WIOInteractionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FWIOInteractionContractTest::RunTest(const FString& Parameters)
{
    FWIOInteractResult Ok = JSONParser::DeserializeWIOInteractResult(
        TEXT("{\"header\":{},\"body\":{\"objectId\":9002,\"success\":true,"
        "\"interactionType\":\"search\",\"channelTimeSec\":0,"
        "\"lootItems\":[{\"itemId\":46,\"quantity\":2},\"oops\"]}}"));
    TestTrue(TEXT("interact success"), Ok.bSuccess);
    TestEqual(TEXT("interact object"), Ok.ObjectId, 9002);
    TestEqual(TEXT("interact type"), Ok.InteractionType, FString(TEXT("search")));
    TestEqual(TEXT("one loot"), Ok.LootItems.Num(), 1);
    if (Ok.LootItems.Num() == 1)
    {
        TestEqual(TEXT("loot item"), Ok.LootItems[0].ItemId, 46);
        TestEqual(TEXT("loot qty"), Ok.LootItems[0].Quantity, 2);
    }

    FWIOStateUpdate St = JSONParser::DeserializeWIOStateUpdate(
        TEXT("{\"header\":{},\"body\":{\"objectId\":9002,\"state\":\"depleted\",\"respawnSec\":300}}"));
    TestEqual(TEXT("state object"), St.ObjectId, 9002);
    TestEqual(TEXT("state new"), St.NewState, EWIOState::Depleted);
    TestEqual(TEXT("state respawn"), St.RespawnSec, 300);

    FWIOChannelCancelled Cancel = JSONParser::DeserializeWIOChannelCancelled(
        TEXT("{\"header\":{},\"body\":{\"objectId\":9002}}"));
    TestEqual(TEXT("cancel object"), Cancel.ObjectId, 9002);

    // Garbage on each entry: defaults + one error log each.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeWIOInteractResult failed to parse JSON"));
    TestFalse(TEXT("garbage interact fails"), JSONParser::DeserializeWIOInteractResult(TEXT("not json")).bSuccess);
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeWIOStateUpdate failed to parse JSON"));
    TestEqual(TEXT("garbage state object default"), JSONParser::DeserializeWIOStateUpdate(TEXT("not json")).ObjectId, 0);
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeWIOChannelCancelled failed to parse JSON"));
    TestEqual(TEXT("garbage cancel default"), JSONParser::DeserializeWIOChannelCancelled(TEXT("not json")).ObjectId, 0);
    return true;
}
