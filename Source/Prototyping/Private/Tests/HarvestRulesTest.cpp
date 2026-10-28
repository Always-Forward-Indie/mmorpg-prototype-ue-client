#include "Misc/AutomationTest.h"
#include "Utils/NetCore/HarvestRules.h"

// Harvest rarity mapping: server payloads carry only rarityId, every display
// consumer must map it the same way. Calls production FHarvestRules directly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarvestRulesTest, "MMO.Harvest.RaritySlug",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FHarvestRulesTest::RunTest(const FString& Parameters)
{
    using R = FHarvestRules;
    TestEqual(TEXT("common"), R::RaritySlugForId(1), FString(TEXT("common")));
    TestEqual(TEXT("uncommon"), R::RaritySlugForId(2), FString(TEXT("uncommon")));
    TestEqual(TEXT("rare"), R::RaritySlugForId(3), FString(TEXT("rare")));
    TestEqual(TEXT("epic"), R::RaritySlugForId(4), FString(TEXT("epic")));
    TestEqual(TEXT("legendary"), R::RaritySlugForId(5), FString(TEXT("legendary")));
    TestEqual(TEXT("unknown defaults"), R::RaritySlugForId(99), FString(TEXT("common")));
    TestEqual(TEXT("zero defaults"), R::RaritySlugForId(0), FString(TEXT("common")));
    return true;
}
