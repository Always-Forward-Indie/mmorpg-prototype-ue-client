#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Data/WIODataStructs.h"
#include "Authentication/LoginFlowTypes.h"
#include "ParserContractHelpers.h"

// Contract specs for the Progression parser domain (roadmap P0.1).
// Same rule as the other domains: fields parse quietly at struct defaults;
// FString entries log one error-level line on garbage/missing body.
// All functions under test are production JSONParser statics.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExperienceUpdateContractTest, "MMO.Protocol.ExperienceUpdateContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FExperienceUpdateContractTest::RunTest(const FString& Parameters)
{
    FExperienceUpdateStruct Valid = JSONParser::DeserializeExperienceUpdate(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"oldLevel\":4,\"newLevel\":5,"
        "\"oldExperience\":1200,\"newExperience\":100,\"experienceChange\":100,"
        "\"expForCurrentLevel\":0,\"expForNextLevel\":1500,\"levelUp\":true,"
        "\"reason\":\"mob_kill\",\"sourceId\":4}}"));
    TestEqual(TEXT("valid char"), Valid.characterId, 9001);
    TestEqual(TEXT("valid new level"), Valid.newLevel, 5);
    TestTrue(TEXT("valid levelup"), Valid.levelUp);
    TestEqual(TEXT("valid reason"), Valid.reason, FString(TEXT("mob_kill")));
    TestEqual(TEXT("valid change"), Valid.experienceChange, 100);

    // Truncated: defaults, no crash.
    FExperienceUpdateStruct Short = JSONParser::DeserializeExperienceUpdate(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001}}"));
    TestEqual(TEXT("short level default"), Short.newLevel, 1);
    TestFalse(TEXT("short levelup default"), Short.levelUp);

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeExperienceUpdate failed to parse JSON"));
    TestEqual(TEXT("garbage char default"), JSONParser::DeserializeExperienceUpdate(TEXT("not json")).characterId, 0);

    // Missing body: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializeExperienceUpdate missing body"));
    TestEqual(TEXT("nobody char default"), JSONParser::DeserializeExperienceUpdate(TEXT("{\"header\":{}}")).characterId, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionContractTest, "MMO.Protocol.ProgressionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FProgressionContractTest::RunTest(const FString& Parameters)
{
    FPlayerProgressionStruct Valid = JSONParser::DeserializePlayerProgression(
        TEXT("{\"header\":{},\"body\":{\"progression\":{\"characterId\":9001,\"currentLevel\":5,"
        "\"currentExperience\":100,\"totalExperience\":1300,\"expForNextLevel\":1500,"
        "\"expForCurrentLevel\":0,\"hasPendingLevelUp\":false,\"pendingLevelGained\":0}}}"));
    TestEqual(TEXT("valid level"), Valid.currentLevel, 5);
    TestEqual(TEXT("valid total"), Valid.totalExperience, 1300);
    TestFalse(TEXT("valid no pending"), Valid.bHasPendingLevelUp);

    // Body without progression object: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializePlayerProgression missing progression"));
    TestEqual(TEXT("noprog level default"), JSONParser::DeserializePlayerProgression(TEXT("{\"header\":{},\"body\":{}}")).currentLevel, 1);

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializePlayerProgression failed to parse JSON"));
    TestEqual(TEXT("garbage level default"), JSONParser::DeserializePlayerProgression(TEXT("not json")).currentLevel, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillsInitContractTest, "MMO.Protocol.SkillsInitContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FSkillsInitContractTest::RunTest(const FString& Parameters)
{
    FPlayerSkillsInitializationData Valid = JSONParser::DeserializePlayerSkillsInitialization(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"skills\":["
        "{\"skillSlug\":\"power_slash\",\"skillLevel\":2,\"castMs\":500,\"coeff\":1.5,"
        "\"cooldownMs\":3000,\"costMp\":20,\"flatAdd\":5,\"gcdMs\":1500,\"maxRange\":300},"
        "\"oops\","
        "{\"skillSlug\":\"battle_cry\"}"
        "]}}"));
    TestEqual(TEXT("valid char"), Valid.characterId, 9001);
    // Wrong-typed entry skipped quietly (type-checked before AsObject).
    TestEqual(TEXT("two skills"), Valid.skills.Num(), 2);
    if (Valid.skills.Num() == 2)
    {
        TestEqual(TEXT("first slug"), Valid.skills[0].skillSlug, FString(TEXT("power_slash")));
        TestEqual(TEXT("first level"), Valid.skills[0].skillLevel, 2);
        TestEqual(TEXT("second slug"), Valid.skills[1].skillSlug, FString(TEXT("battle_cry")));
        TestEqual(TEXT("second level default"), Valid.skills[1].skillLevel, 1);
    }

    // Garbage: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializePlayerSkillsInitialization failed to parse JSON"));
    TestEqual(TEXT("garbage char default"), JSONParser::DeserializePlayerSkillsInitialization(TEXT("not json")).characterId, 0);

    // Missing body: default + one error log.
    AddExpectedErrorPlain(TEXT("JSONParser: DeserializePlayerSkillsInitialization missing body"));
    TestEqual(TEXT("nobody skills empty"), JSONParser::DeserializePlayerSkillsInitialization(TEXT("{\"header\":{}}")).skills.Num(), 0);
    return true;
}

// Quest reward notifications (chunk sendFacts): the server keys are snake_case
// (item_slug); the parsers accept camelCase/legacy aliases so old servers and
// quest/dialogue/vendor paths converge in one place.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestRewardNotifContractTest, "MMO.Protocol.QuestRewardNotifContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FQuestRewardNotifContractTest::RunTest(const FString& Parameters)
{
    // Server wire shape: item_slug (underscore), no display name.
    TSharedPtr<FJsonObject> ItemBody = MakeShareable(new FJsonObject());
    ItemBody->SetNumberField(TEXT("itemId"), 46);
    ItemBody->SetStringField(TEXT("item_slug"), TEXT("wolf_fang"));
    ItemBody->SetNumberField(TEXT("quantity"), 2);
    FItemReceivedData Item = JSONParser::DeserializeQuestItemReceived(ItemBody);
    TestEqual(TEXT("item id"), Item.itemId, 46);
    TestEqual(TEXT("item slug"), Item.itemSlug, FString(TEXT("wolf_fang")));
    TestEqual(TEXT("item qty"), Item.quantity, 2);
    TestTrue(TEXT("item name empty ok"), Item.itemName.IsEmpty());

    // Legacy camelCase keys still parse (old servers / other paths).
    TSharedPtr<FJsonObject> LegacyBody = MakeShareable(new FJsonObject());
    LegacyBody->SetNumberField(TEXT("itemId"), 7);
    LegacyBody->SetStringField(TEXT("itemSlug"), TEXT("iron_sword"));
    LegacyBody->SetStringField(TEXT("itemName"), TEXT("Iron Sword"));
    LegacyBody->SetNumberField(TEXT("quantity"), 1);
    FItemReceivedData Legacy = JSONParser::DeserializeQuestItemReceived(LegacyBody);
    TestEqual(TEXT("legacy slug"), Legacy.itemSlug, FString(TEXT("iron_sword")));
    TestEqual(TEXT("legacy name"), Legacy.itemName, FString(TEXT("Iron Sword")));

    // Empty body: quiet defaults, no crash.
    FItemReceivedData Empty = JSONParser::DeserializeQuestItemReceived(MakeShareable(new FJsonObject()));
    TestEqual(TEXT("empty item id"), Empty.itemId, 0);
    TestTrue(TEXT("empty item slug"), Empty.itemSlug.IsEmpty());

    TSharedPtr<FJsonObject> ExpBody = MakeShareable(new FJsonObject());
    ExpBody->SetNumberField(TEXT("amount"), 150);
    FExpReceivedData Exp = JSONParser::DeserializeQuestExpReceived(ExpBody);
    TestEqual(TEXT("exp amount"), Exp.amount, 150);
    TestTrue(TEXT("exp source empty ok"), Exp.source.IsEmpty());

    TSharedPtr<FJsonObject> GoldBody = MakeShareable(new FJsonObject());
    GoldBody->SetNumberField(TEXT("amount"), 25);
    GoldBody->SetStringField(TEXT("questSlug"), TEXT("wolf_hunt"));
    FGoldReceivedData Gold = JSONParser::DeserializeQuestGoldReceived(GoldBody);
    TestEqual(TEXT("gold amount"), Gold.amount, 25);
    TestEqual(TEXT("gold source"), Gold.source, FString(TEXT("wolf_hunt")));
    return true;
}
