#include "Misc/AutomationTest.h"
#include "Gameplay/Items/ItemManager.h"
#include "Utils/JSONParser.h"
#include "ParserContractHelpers.h"

// S2.1 contract: versioned item stat catalog. Same-version chunks accumulate,
// a new version replaces. Catalog entries reuse the inventory-record wrapper
// ({itemId, item:{...}}) so the shared parser applies with no new code.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FItemCatalogCacheContractTest, "MMO.Protocol.ItemCatalogCacheContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemCatalogCacheContractTest::RunTest(const FString& Parameters)
{
    UItemManager* Mgr = NewObject<UItemManager>(GetTransientPackage());
    TestTrue(TEXT("manager created"), Mgr != nullptr);
    if (!Mgr) return false;

    TestEqual(TEXT("version starts unknown"), Mgr->GetCatalogVersion(), 0);
    FInventoryItemStruct Miss;
    TestFalse(TEXT("miss before catalog"), Mgr->GetCatalogStats(46, Miss));

    // First chunk of v7: two templates.
    TArray<FInventoryItemStruct> Chunk1;
    FInventoryItemStruct A; A.itemId = 46; A.slug = TEXT("health_potion"); A.weight = 0.1f;
    FInventoryItemStruct B; B.itemId = 2; B.slug = TEXT("old_oak_shield"); B.weight = 3.0f;
    Chunk1.Add(A); Chunk1.Add(B);
    Mgr->SetItemCatalog(7, Chunk1);
    TestEqual(TEXT("version cached"), Mgr->GetCatalogVersion(), 7);

    FInventoryItemStruct Got;
    TestTrue(TEXT("hit 46"), Mgr->GetCatalogStats(46, Got));
    TestEqual(TEXT("slug 46"), Got.slug, FString(TEXT("health_potion")));
    TestTrue(TEXT("hit 2"), Mgr->GetCatalogStats(2, Got));

    // Second chunk, same version: accumulates.
    TArray<FInventoryItemStruct> Chunk2;
    FInventoryItemStruct C; C.itemId = 5; C.slug = TEXT("wolf_fang");
    Chunk2.Add(C);
    Mgr->SetItemCatalog(7, Chunk2);
    TestTrue(TEXT("accumulated 5"), Mgr->GetCatalogStats(5, Got));
    TestTrue(TEXT("kept 46"), Mgr->GetCatalogStats(46, Got));

    // New version: replaces.
    TArray<FInventoryItemStruct> Chunk3;
    FInventoryItemStruct D; D.itemId = 9; D.slug = TEXT("iron_sword");
    Chunk3.Add(D);
    Mgr->SetItemCatalog(8, Chunk3);
    TestEqual(TEXT("version replaced"), Mgr->GetCatalogVersion(), 8);
    TestFalse(TEXT("old entry gone"), Mgr->GetCatalogStats(46, Got));
    TestTrue(TEXT("new entry kept"), Mgr->GetCatalogStats(9, Got));

    // Catalog wire entry parses through the shared inventory parser.
    FInventoryItemStruct Wire = JSONParser::DeserializeInventoryItem(ParserContract_ParseObj(
        TEXT("{\"itemId\":46,\"quantity\":0,\"item\":{\"id\":46,\"slug\":\"health_potion\","
        "\"itemTypeSlug\":\"consumable\",\"raritySlug\":\"common\",\"weight\":0.1,"
        "\"stackMax\":99,\"vendorPriceBuy\":10,\"vendorPriceSell\":3,"
        "\"attributes\":[{\"slug\":\"weight\",\"value\":0.1}],\"useEffects\":[]}}")));
    TestEqual(TEXT("wire itemId"), Wire.itemId, 46);
    TestEqual(TEXT("wire slug"), Wire.slug, FString(TEXT("health_potion")));
    return true;
}
