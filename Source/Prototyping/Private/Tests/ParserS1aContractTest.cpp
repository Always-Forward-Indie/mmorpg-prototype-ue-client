#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// S1a contract: server omits display strings (name/*Name/*String); client
// resolves display text via slug (locale/humanized) and routes logic via ints.
// These specs pin the tolerant-parser side of that contract: missing strings
// parse quietly at struct defaults, slugs and int enums survive.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHumanizeSlugContractTest, "MMO.Protocol.HumanizeSlugContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHumanizeSlugContractTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("underscores"), JSONParser::HumanizeSlugForDisplay(TEXT("forest_wolf")), FString(TEXT("Forest wolf")));
    TestEqual(TEXT("single"), JSONParser::HumanizeSlugForDisplay(TEXT("wolf")), FString(TEXT("Wolf")));
    TestTrue(TEXT("empty stays empty"), JSONParser::HumanizeSlugForDisplay(TEXT("")).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FS1aMobNoNameContractTest, "MMO.Protocol.S1aMobNoNameContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FS1aMobNoNameContractTest::RunTest(const FString& Parameters)
{
    // New-shape spawn entry: no "name", slug-only attributes.
    FMOBStruct Mob = JSONParser::DeserializeMobData(ParserContract_ParseObj(
        TEXT("{\"id\":5,\"uid\":4242,\"zoneId\":9001,\"slug\":\"RuinSkeleton\",\"race\":\"undead\","
        "\"level\":1,\"isAggressive\":false,\"isDead\":false,"
        "\"stats\":{\"health\":{\"current\":70,\"max\":70},\"mana\":{\"current\":5,\"max\":5}},"
        "\"position\":{\"x\":1,\"y\":2,\"z\":3,\"rotationZ\":0},"
        "\"velocity\":{\"dirX\":0,\"dirY\":0,\"speed\":0},"
        "\"attributes\":[{\"slug\":\"max_health\",\"value\":70}]}")));
    TestTrue(TEXT("name empty, no crash"), Mob.mobName.IsEmpty());
    TestEqual(TEXT("slug kept"), Mob.mobSlug, FString(TEXT("RuinSkeleton")));
    TestEqual(TEXT("id kept"), Mob.mobID, 5);
    const FAttributeDataStruct* Health = Mob.mobAttributes.attributesData.Find(TEXT("max_health"));
    TestTrue(TEXT("attr by slug"), Health != nullptr);
    if (Health)
    {
        TestEqual(TEXT("attr value"), Health->attributeValue, 70);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FS1aNPCNoNameContractTest, "MMO.Protocol.S1aNPCNoNameContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FS1aNPCNoNameContractTest::RunTest(const FString& Parameters)
{
    FNPCStruct NPC = JSONParser::DeserializeNPCData(ParserContract_ParseObj(
        TEXT("{\"id\":2,\"slug\":\"milaya\",\"race\":\"human\",\"level\":1,\"npcType\":3,"
        "\"isInteractable\":true,\"dialogueId\":1,\"quests\":[{\"slug\":\"wolf_hunt\",\"status\":\"available\"}],"
        "\"stats\":{\"health\":{\"current\":100,\"max\":100},\"mana\":{\"current\":50,\"max\":50}},"
        "\"position\":{\"x\":1,\"y\":2,\"z\":3,\"rotationZ\":0},\"attributes\":[]}")));
    TestTrue(TEXT("name empty, no crash"), NPC.name.IsEmpty());
    TestEqual(TEXT("slug kept"), NPC.slug, FString(TEXT("milaya")));
    TestEqual(TEXT("id kept"), NPC.id, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FS1aCombatNoStringsContractTest, "MMO.Protocol.S1aCombatNoStringsContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FS1aCombatNoStringsContractTest::RunTest(const FString& Parameters)
{
    // New-shape initiation: ints + slug only, no skillName/*TypeString/animationName.
    FSkillInitiationData Init = JSONParser::DeserializeSkillInitiation(ParserContract_ParseObj(
        TEXT("{\"success\":true,\"casterId\":7,\"targetId\":4242,\"targetType\":3,"
        "\"skillSlug\":\"fireball\",\"skillEffectType\":\"damage\",\"skillSchool\":\"fire\","
        "\"castTime\":0.5,\"casterType\":2,\"cooldownMs\":1500,\"gcdMs\":1000,"
        "\"serverTimestamp\":1729765432123,\"castStartedAt\":1729765432000}")));
    TestTrue(TEXT("init success"), Init.success);
    TestTrue(TEXT("skillName empty"), Init.skillName.IsEmpty());
    TestTrue(TEXT("type strings empty"), Init.targetTypeString.IsEmpty() && Init.casterTypeString.IsEmpty());
    TestEqual(TEXT("skillSlug kept"), Init.skillSlug, FString(TEXT("fireball")));
    TestEqual(TEXT("casterType int kept"), Init.casterType, 2);
    TestEqual(TEXT("targetType int kept"), Init.targetType, 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FS1aItemNoNamesContractTest, "MMO.Protocol.S1aItemNoNamesContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FS1aItemNoNamesContractTest::RunTest(const FString& Parameters)
{
    // New-shape drop template: no *Name keys, slug-only attributes.
    FItemBaseStruct Item = JSONParser::DeserializeItemData(ParserContract_ParseObj(
        TEXT("{\"id\":46,\"slug\":\"wolf_fang\",\"isQuestItem\":false,"
        "\"itemTypeSlug\":\"material\",\"raritySlug\":\"common\",\"equipSlotSlug\":\"\","
        "\"attributes\":[{\"slug\":\"weight\",\"value\":0.1}]}")));
    TestEqual(TEXT("slug kept"), Item.slug, FString(TEXT("wolf_fang")));
    TestEqual(TEXT("type slug kept"), Item.itemTypeSlug, FString(TEXT("material")));
    TestEqual(TEXT("one attribute"), Item.attributes.Num(), 1);
    if (Item.attributes.Num() == 1)
    {
        TestEqual(TEXT("attr slug"), Item.attributes[0].slug, FString(TEXT("weight")));
        TestTrue(TEXT("attr name empty"), Item.attributes[0].name.IsEmpty());
    }
    return true;
}
