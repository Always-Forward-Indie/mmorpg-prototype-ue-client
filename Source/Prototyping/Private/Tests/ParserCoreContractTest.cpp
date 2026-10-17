#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// Contract specs for the Core/Auth parser entry points (roadmap P0.1).
// Rule pinned here: every FString entry never crashes — garbage JSON or a
// missing body yields a default struct/empty list plus ONE error-level log
// (malformed server packet = seam failure). Wrong-typed or missing FIELDS
// stay at struct defaults quietly (no per-field log spam). Unknown extra
// fields are ignored. All functions under test are production JSONParser
// statics; JSON text -> FJsonObject parsing below is engine API, not game
// logic. Error-logging cases declare AddExpectedErrorPlain, otherwise the
// harness fails the spec.

namespace
{
TSharedPtr<FJsonObject> ParseObj(const FString& Json)
{
    TSharedPtr<FJsonObject> Out;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    FJsonSerializer::Deserialize(Reader, Out);
    return Out;
}

TArray<TSharedPtr<FJsonValue>> ParseArray(const FString& Json)
{
    TSharedPtr<FJsonObject> Root = ParseObj(Json);
    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Root.IsValid() && Root->TryGetArrayField(TEXT("items"), Arr) && Arr)
    {
        return *Arr;
    }
    return {};
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterDataContractTest, "MMO.Protocol.CharacterDataContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCharacterDataContractTest::RunTest(const FString& Parameters)
{
    // Valid packet with an unknown future field: parses, extras ignored.
    FCharacterDataStruct Valid = JSONParser::DeserializeCharacterData(
        TEXT("{\"header\":{},\"body\":{\"character\":{\"id\":7,\"name\":\"Bob\",\"class\":\"warrior\",\"race\":\"human\",\"gender\":\"male\",\"level\":5,\"isDead\":false,"
        "\"position\":{\"x\":1,\"y\":2,\"z\":3,\"rotationZ\":90},\"futureField\":123}}}"));
    TestEqual(TEXT("valid id"), Valid.characterId, 7);
    TestEqual(TEXT("valid name"), Valid.characterName, FString(TEXT("Bob")));
    TestEqual(TEXT("valid level"), Valid.characterLevel, 5);
    TestFalse(TEXT("valid alive"), Valid.bIsDead);
    TestEqual(TEXT("valid pos x"), Valid.characterPosition.positionX, 1.0);
    TestEqual(TEXT("valid rot"), Valid.characterPosition.rotationZ, 90.0);

    // Garbage in: default struct + one error log, no crash.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeCharacterData failed to parse JSON"));
    FCharacterDataStruct Garbage = JSONParser::DeserializeCharacterData(TEXT("not json"));
    TestEqual(TEXT("garbage id default"), Garbage.characterId, 0);
    TestTrue(TEXT("garbage name default"), Garbage.characterName.IsEmpty());

    // Missing body: default struct + one error log, no crash (used to AV).
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeCharacterData missing body/character"));
    FCharacterDataStruct NoBody = JSONParser::DeserializeCharacterData(TEXT("{\"header\":{}}"));
    TestEqual(TEXT("nobody id default"), NoBody.characterId, 0);

    // Body without character object: same contract.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeCharacterData missing body/character"));
    FCharacterDataStruct NoChar = JSONParser::DeserializeCharacterData(TEXT("{\"header\":{},\"body\":{}}"));
    TestEqual(TEXT("nochar id default"), NoChar.characterId, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPositionContractTest, "MMO.Protocol.PositionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FPositionContractTest::RunTest(const FString& Parameters)
{
    FPositionDataStruct Valid = JSONParser::DeserializePositionData(
        ParseObj(TEXT("{\"x\":10,\"y\":20,\"z\":30,\"rotationZ\":45}")));
    TestEqual(TEXT("valid x"), Valid.positionX, 10.0);
    TestEqual(TEXT("valid z"), Valid.positionZ, 30.0);
    TestEqual(TEXT("valid rot"), Valid.rotationZ, 45.0);

    // Truncated tail: missing fields stay 0, no crash, no log spam.
    FPositionDataStruct Short = JSONParser::DeserializePositionData(
        ParseObj(TEXT("{\"y\":20}")));
    TestEqual(TEXT("short x default"), Short.positionX, 0.0);
    TestEqual(TEXT("short y kept"), Short.positionY, 20.0);

    // Wrong-typed field: default for that field only, rest intact.
    FPositionDataStruct Wrong = JSONParser::DeserializePositionData(
        ParseObj(TEXT("{\"x\":\"oops\",\"y\":20,\"z\":30,\"rotationZ\":45}")));
    TestEqual(TEXT("wrong x default"), Wrong.positionX, 0.0);
    TestEqual(TEXT("wrong y kept"), Wrong.positionY, 20.0);

    // Null object: zeros, no crash.
    FPositionDataStruct Null = JSONParser::DeserializePositionData(nullptr);
    TestEqual(TEXT("null x default"), Null.positionX, 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttributesContractTest, "MMO.Protocol.AttributesContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FAttributesContractTest::RunTest(const FString& Parameters)
{
    TArray<TSharedPtr<FJsonValue>> Arr = ParseArray(
        TEXT("{\"items\":["
        "{\"id\":1,\"name\":\"Strength\",\"slug\":\"str\",\"value\":10},"
        "{\"id\":2,\"name\":\"NoSlug\",\"value\":5},"
        "42,"
        "{\"id\":3,\"slug\":\"agi\",\"value\":\"oops\"}"
        "]}"));
    TMap<FString, FAttributeDataStruct> Map = JSONParser::DeserializeAttributesArray(Arr);
    // Slugless entry and the non-object are skipped; wrong-typed value -> 0.
    TestEqual(TEXT("two slugged entries"), Map.Num(), 2);
    TestTrue(TEXT("str present"), Map.Contains(TEXT("str")));
    TestTrue(TEXT("agi present"), Map.Contains(TEXT("agi")));
    if (Map.Contains(TEXT("str")))
    {
        TestEqual(TEXT("str value"), Map[TEXT("str")].attributeValue, 10);
    }
    if (Map.Contains(TEXT("agi")))
    {
        TestEqual(TEXT("agi wrong-type defaults"), Map[TEXT("agi")].attributeValue, 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharactersListContractTest, "MMO.Protocol.CharactersListContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCharactersListContractTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> Body = ParseObj(
        TEXT("{\"characters\":["
        "{\"clientId\":11,\"character\":{\"id\":7,\"name\":\"Bob\",\"level\":5}},"
        "{\"clientId\":12},"
        "{\"clientId\":\"oops\",\"character\":{\"id\":9,\"name\":\"Zed\"}}"
        "]}"));
    TArray<FClientDataStruct> List = JSONParser::DeserializeCharactersList(Body);
    TestEqual(TEXT("three entries kept"), List.Num(), 3);
    if (List.Num() == 3)
    {
        TestEqual(TEXT("first client"), List[0].clientId, 11);
        TestEqual(TEXT("first char"), List[0].characterData.characterId, 7);
        // Missing character object: default char data, client id kept.
        TestEqual(TEXT("second client"), List[1].clientId, 12);
        TestEqual(TEXT("second char default"), List[1].characterData.characterId, 0);
        // Wrong-typed clientId: 0, character still parsed.
        TestEqual(TEXT("third client default"), List[2].clientId, 0);
        TestEqual(TEXT("third char kept"), List[2].characterData.characterId, 9);
    }

    // Missing array: empty, no crash.
    TestEqual(TEXT("no array empty"), JSONParser::DeserializeCharactersList(ParseObj(TEXT("{}"))).Num(), 0);
    // Null body: empty, no crash.
    TestEqual(TEXT("null body empty"), JSONParser::DeserializeCharactersList(nullptr).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoginListContractTest, "MMO.Protocol.LoginListContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FLoginListContractTest::RunTest(const FString& Parameters)
{
    TArray<FLoginCharacterEntry> Valid = JSONParser::DeserializeLoginCharactersList(
        TEXT("{\"header\":{},\"body\":{\"charactersList\":["
        "{\"characterId\":3,\"characterName\":\"Cara\",\"characterLevel\":4,\"classSlug\":\"mage\",\"raceSlug\":\"elf\",\"genderSlug\":\"female\","
        "\"equipment\":[{\"slotId\":1,\"itemSlug\":\"staff_fire\"}],\"unknownFuture\":true}"
        "]}}"));
    TestEqual(TEXT("one entry"), Valid.Num(), 1);
    if (Valid.Num() == 1)
    {
        TestEqual(TEXT("id"), Valid[0].CharacterId, 3);
        TestEqual(TEXT("name"), Valid[0].CharacterName, FString(TEXT("Cara")));
        TestEqual(TEXT("class slug"), Valid[0].CharacterClass, FString(TEXT("mage")));
        TestEqual(TEXT("equip count"), Valid[0].Equipment.Num(), 1);
        if (Valid[0].Equipment.Num() == 1)
        {
            TestEqual(TEXT("equip slug"), Valid[0].Equipment[0].ItemSlug, FString(TEXT("staff_fire")));
        }
    }

    // Garbage: empty + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeLoginCharactersList failed to parse JSON"));
    TestEqual(TEXT("garbage empty"), JSONParser::DeserializeLoginCharactersList(TEXT("not json")).Num(), 0);

    // Missing body: empty + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeLoginCharactersList missing body"));
    TestEqual(TEXT("nobody empty"), JSONParser::DeserializeLoginCharactersList(TEXT("{\"header\":{}}")).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharCreateOptionsContractTest, "MMO.Protocol.CharCreateOptionsContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCharCreateOptionsContractTest::RunTest(const FString& Parameters)
{
    FCharacterCreationOptions Valid = JSONParser::DeserializeCharacterCreationOptions(
        TEXT("{\"header\":{},\"body\":{"
        "\"classes\":[{\"id\":1,\"name\":\"Warrior\",\"slug\":\"warrior\",\"description\":\"Fights\"}],"
        "\"races\":[{\"id\":2,\"name\":\"Elf\",\"slug\":\"elf\"}],"
        "\"genders\":[{\"id\":3,\"slug\":\"female\",\"label\":\"Female\"}]}}"));
    TestTrue(TEXT("valid complete"), Valid.IsValid());
    TestEqual(TEXT("one class"), Valid.Classes.Num(), 1);
    TestEqual(TEXT("one race"), Valid.Races.Num(), 1);
    TestEqual(TEXT("one gender"), Valid.Genders.Num(), 1);
    if (Valid.Classes.Num() == 1)
    {
        TestEqual(TEXT("class slug"), Valid.Classes[0].Slug, FString(TEXT("warrior")));
    }

    // Garbage: invalid + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeCharacterCreationOptions failed to parse JSON"));
    TestFalse(TEXT("garbage invalid"), JSONParser::DeserializeCharacterCreationOptions(TEXT("not json")).IsValid());

    // Missing body: invalid + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeCharacterCreationOptions missing body"));
    TestFalse(TEXT("nobody invalid"), JSONParser::DeserializeCharacterCreationOptions(TEXT("{\"header\":{}}")).IsValid());
    return true;
}
