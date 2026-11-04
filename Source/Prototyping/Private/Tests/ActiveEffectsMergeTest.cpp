#include "Misc/AutomationTest.h"
#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "StatsUpdateCounterHelper.h"

// Convergence specs for the HUD/characteristics effects snapshot.
// End-to-end through production code only (no re-implementation):
// wire JSON -> JSONParser (prod) -> UPlayerStatsManager (prod).
//   - full stats_update packets replace the list (server-authoritative removal),
//   - partial packets merge by composite key so multi-attribute slugs survive,
//   - ApplyActiveEffects (dedicated setPlayerActiveEffects path) refreshes the
//     same cache AND notifies OnStatsUpdated listeners (HUD reads that bus).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActiveEffectsMergeTest, "MMO.Protocol.ActiveEffectsMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FActiveEffectsMergeTest::RunTest(const FString& Parameters)
{
    UPlayerStatsManager* Mgr = NewObject<UPlayerStatsManager>(GetTransientPackage());
    TestNotNull(TEXT("manager created"), Mgr);
    if (!Mgr)
    {
        return false;
    }

    UStatsUpdateCounter* Counter = NewObject<UStatsUpdateCounter>(GetTransientPackage());
    Mgr->OnStatsUpdated.AddDynamic(Counter, &UStatsUpdateCounter::OnUpdate);

    // 1. Full packet: title + mastery both land (parsed by production JSONParser).
    Mgr->ApplyStatsUpdate(JSONParser::DeserializePlayerStatsUpdate(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"level\":5,"
        "\"health\":{\"current\":80,\"max\":100},\"mana\":{\"current\":10,\"max\":50},"
        "\"attributes\":[{\"slug\":\"physical_attack\",\"name\":\"Physical Attack\",\"base\":10,\"effective\":11}],"
        "\"activeEffects\":["
        "{\"slug\":\"title_first_hunter_physical_attack\",\"effectTypeSlug\":\"title_bonus\","
        "\"attributeSlug\":\"physical_attack\",\"sourceType\":\"title\",\"value\":1,\"expiresAt\":0},"
        "{\"slug\":\"staff_mastery_t1_damage\",\"effectTypeSlug\":\"buff\","
        "\"attributeSlug\":\"magical_attack\",\"sourceType\":\"mastery\",\"value\":1,\"expiresAt\":0}"
        "]}}")));
    TestEqual(TEXT("full two effects"), Mgr->GetCachedStats().activeEffects.Num(), 2);

    // 2. Partial refresh of the same composite key updates value, keeps count.
    Mgr->ApplyStatsUpdate(JSONParser::DeserializePlayerStatsUpdate(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"level\":5,"
        "\"health\":{\"current\":80,\"max\":100},\"mana\":{\"current\":10,\"max\":50},"
        "\"activeEffects\":["
        "{\"slug\":\"title_first_hunter_physical_attack\",\"effectTypeSlug\":\"title_bonus\","
        "\"attributeSlug\":\"physical_attack\",\"sourceType\":\"title\",\"value\":2,\"expiresAt\":0}"
        "]}}")));
    TestEqual(TEXT("partial keeps count"), Mgr->GetCachedStats().activeEffects.Num(), 2);

    // 3. Same slug, different attribute: must NOT collapse (old slug-only merge lost it).
    Mgr->ApplyStatsUpdate(JSONParser::DeserializePlayerStatsUpdate(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"level\":5,"
        "\"health\":{\"current\":80,\"max\":100},\"mana\":{\"current\":10,\"max\":50},"
        "\"activeEffects\":["
        "{\"slug\":\"title_first_hunter_physical_attack\",\"effectTypeSlug\":\"title_bonus\","
        "\"attributeSlug\":\"magical_attack\",\"sourceType\":\"title\",\"value\":3,\"expiresAt\":0}"
        "]}}")));
    TestEqual(TEXT("composite key keeps three"), Mgr->GetCachedStats().activeEffects.Num(), 3);

    // 4. Dedicated effects path converges the same cache and notifies HUD bus.
    const int32 CountBefore = Counter->Count;
    Mgr->ApplyActiveEffects(JSONParser::DeserializeActiveEffectsPacket(
        TEXT("{\"header\":{},\"body\":{\"activeEffects\":["
        "{\"slug\":\"staff_mastery_t1_damage\",\"effectTypeSlug\":\"buff\","
        "\"attributeSlug\":\"magical_attack\",\"sourceType\":\"mastery\",\"value\":1,\"expiresAt\":0}"
        "]}}")));
    TestEqual(TEXT("dedicated replaces slice"), Mgr->GetCachedStats().activeEffects.Num(), 1);
    TestEqual(TEXT("dedicated slug"), Mgr->GetCachedStats().activeEffects[0].slug, FString(TEXT("staff_mastery_t1_damage")));
    TestTrue(TEXT("dedicated notifies stats bus"), Counter->Count > CountBefore);
    TestEqual(TEXT("bus carries one effect"), Counter->LastEffects, 1);

    // 5. Full packet without the title removes it (server-authoritative removal).
    Mgr->ApplyStatsUpdate(JSONParser::DeserializePlayerStatsUpdate(
        TEXT("{\"header\":{},\"body\":{\"characterId\":9001,\"level\":5,"
        "\"health\":{\"current\":80,\"max\":100},\"mana\":{\"current\":10,\"max\":50},"
        "\"attributes\":[{\"slug\":\"physical_attack\",\"name\":\"Physical Attack\",\"base\":10,\"effective\":10}],"
        "\"activeEffects\":["
        "{\"slug\":\"staff_mastery_t1_damage\",\"effectTypeSlug\":\"buff\","
        "\"attributeSlug\":\"magical_attack\",\"sourceType\":\"mastery\",\"value\":1,\"expiresAt\":0}"
        "]}}")));
    TestEqual(TEXT("full removal honored"), Mgr->GetCachedStats().activeEffects.Num(), 1);
    TestEqual(TEXT("remaining is mastery"), Mgr->GetCachedStats().activeEffects[0].sourceType, FString(TEXT("mastery")));
    return true;
}
