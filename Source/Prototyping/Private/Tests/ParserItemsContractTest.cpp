#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// Contract specs for the Items parser domain (roadmap P0.1).
// Same rule as Core/Mobs/Harvest: fields parse quietly at struct defaults;
// wrong-typed list entries skip without LogJson spam. All functions under
// test are production JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemDataContractTest, "MMO.Protocol.ItemDataContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemDataContractTest::RunTest(const FString& Parameters)
{
    FItemBaseStruct Valid = JSONParser::DeserializeItemData(ParserContract_ParseObj(
        TEXT("{\"id\":46,\"name\":\"Wolf Fang\",\"slug\":\"wolf_fang\",\"description\":\"Sharp\","
        "\"isQuestItem\":false,\"itemTypeName\":\"Material\",\"itemTypeSlug\":\"material\","
        "\"attributes\":[{\"id\":1,\"name\":\"Weight\",\"slug\":\"weight\",\"value\":0.1},\"oops\"]}")));
    TestEqual(TEXT("valid id"), Valid.id, 46);
    TestEqual(TEXT("valid name"), Valid.name, FString(TEXT("Wolf Fang")));
    TestEqual(TEXT("valid slug"), Valid.slug, FString(TEXT("wolf_fang")));
    TestEqual(TEXT("valid typename"), Valid.itemTypeName, FString(TEXT("Material")));
    // Wrong-typed attribute entry skipped quietly (no LogJson Error).
    TestEqual(TEXT("one attribute"), Valid.attributes.Num(), 1);
    if (Valid.attributes.Num() == 1)
    {
        TestEqual(TEXT("attr slug"), Valid.attributes[0].slug, FString(TEXT("weight")));
    }

    // Truncated: defaults, no crash.
    FItemBaseStruct Short = JSONParser::DeserializeItemData(ParserContract_ParseObj(TEXT("{\"id\":47}")));
    TestEqual(TEXT("short id"), Short.id, 47);
    TestTrue(TEXT("short name empty"), Short.name.IsEmpty());

    // Null: defaults, no crash.
    FItemBaseStruct Null = JSONParser::DeserializeItemData(nullptr);
    TestEqual(TEXT("null id default"), Null.id, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDroppedItemContractTest, "MMO.Protocol.DroppedItemContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDroppedItemContractTest::RunTest(const FString& Parameters)
{
    FDroppedItemStruct Valid = JSONParser::DeserializeDroppedItem(ParserContract_ParseObj(
        TEXT("{\"uid\":701,\"itemId\":46,\"droppedByMobUID\":501,\"droppedByCharacterId\":0,"
        "\"quantity\":2,\"canBePickedUp\":true,\"reservationSecondsLeft\":30,"
        "\"position\":{\"x\":10,\"y\":20,\"z\":5,\"rotationZ\":0},"
        "\"item\":{\"id\":46,\"name\":\"Wolf Fang\",\"slug\":\"wolf_fang\"}}")));
    TestEqual(TEXT("valid uid"), Valid.uid, 701);
    TestEqual(TEXT("valid item"), Valid.itemId, 46);
    TestEqual(TEXT("valid mob"), Valid.droppedByMobUID, FString(TEXT("501")));
    TestEqual(TEXT("valid qty"), Valid.quantity, 2);
    TestTrue(TEXT("valid pickup"), Valid.canBePickedUp);
    TestEqual(TEXT("valid pos"), Valid.position.positionX, 10.0);
    TestEqual(TEXT("valid nested"), Valid.item.slug, FString(TEXT("wolf_fang")));

    // Server "no mob" spellings (0 / "0" / "") all normalize to empty.
    FDroppedItemStruct ZeroNum = JSONParser::DeserializeDroppedItem(ParserContract_ParseObj(TEXT("{\"droppedByMobUID\":0}")));
    TestTrue(TEXT("zero num empty"), ZeroNum.droppedByMobUID.IsEmpty());
    FDroppedItemStruct ZeroStr = JSONParser::DeserializeDroppedItem(ParserContract_ParseObj(TEXT("{\"droppedByMobUID\":\"0\"}")));
    TestTrue(TEXT("zero str empty"), ZeroStr.droppedByMobUID.IsEmpty());

    // Truncated: struct defaults (qty 1, pickup true), no crash.
    FDroppedItemStruct Short = JSONParser::DeserializeDroppedItem(ParserContract_ParseObj(TEXT("{\"uid\":702}")));
    TestEqual(TEXT("short qty default"), Short.quantity, 1);
    TestTrue(TEXT("short pickup default"), Short.canBePickedUp);

    // Null: defaults, no crash.
    FDroppedItemStruct Null = JSONParser::DeserializeDroppedItem(nullptr);
    TestEqual(TEXT("null uid default"), Null.uid, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemDropResponseContractTest, "MMO.Protocol.ItemDropResponseContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemDropResponseContractTest::RunTest(const FString& Parameters)
{
    // Current "items" array shape with one wrong-typed entry.
    FItemDropResponseStruct Cur = JSONParser::DeserializeItemDropResponse(ParserContract_ParseObj(
        TEXT("{\"items\":[{\"uid\":701,\"itemId\":46},\"oops\",{\"uid\":702,\"itemId\":47}]}")));
    TestEqual(TEXT("current two items"), Cur.droppedItems.Num(), 2);
    if (Cur.droppedItems.Num() == 2)
    {
        TestEqual(TEXT("first uid"), Cur.droppedItems[0].uid, 701);
        TestEqual(TEXT("second uid"), Cur.droppedItems[1].uid, 702);
    }

    // Legacy "droppedItems" array shape still resolves.
    FItemDropResponseStruct Legacy = JSONParser::DeserializeItemDropResponse(ParserContract_ParseObj(
        TEXT("{\"droppedItems\":[{\"uid\":703,\"itemId\":48}]}")));
    TestEqual(TEXT("legacy one item"), Legacy.droppedItems.Num(), 1);

    // Missing arrays and null body: empty, no crash.
    TestEqual(TEXT("no array empty"), JSONParser::DeserializeItemDropResponse(ParserContract_ParseObj(TEXT("{}"))).droppedItems.Num(), 0);
    TestEqual(TEXT("null empty"), JSONParser::DeserializeItemDropResponse(nullptr).droppedItems.Num(), 0);
    return true;
}
