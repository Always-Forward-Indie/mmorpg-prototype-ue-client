#include "Misc/AutomationTest.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Vendor/VendorNetworkHandler.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDropResolveContractTest, "MMO.Protocol.DropResolveContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDropResolveContractTest::RunTest(const FString& Parameters)
{
    UItemManager* Mgr = NewObject<UItemManager>(GetTransientPackage());
    TestTrue(TEXT("manager created"), Mgr != nullptr);
    if (!Mgr) return false;

    // Seed catalog: template 46 with a typed attribute.
    FInventoryItemStruct Tpl;
    Tpl.itemId = 46;
    Tpl.slug = TEXT("health_potion");
    Tpl.itemTypeSlug = TEXT("consumable");
    Tpl.item_type_id = 3;
    FItemAttributeStruct Attr;
    Attr.slug = TEXT("weight");
    Attr.value = 0.1f;
    Tpl.itemAttributes.Add(Attr);
    Mgr->SetItemCatalog(7, { Tpl });

    // Slim id-only drop parses with an empty template...
    FDroppedItemStruct Drop = JSONParser::DeserializeDroppedItem(ParserContract_ParseObj(
        TEXT("{\"uid\":701,\"itemId\":46,\"quantity\":2,\"canBePickedUp\":true,"
        "\"position\":{\"x\":10,\"y\":20,\"z\":5,\"rotationZ\":0}}")));
    TestTrue(TEXT("template empty pre-resolve"), Drop.item.slug.IsEmpty());

    // ...and resolves from the catalog.
    Mgr->ResolveDropStats(Drop);
    TestEqual(TEXT("resolved slug"), Drop.item.slug, FString(TEXT("health_potion")));
    TestEqual(TEXT("resolved id"), Drop.item.id, 46);
    TestEqual(TEXT("resolved type"), (int32)Drop.item.itemType, 3);
    TestEqual(TEXT("resolved type slug"), Drop.item.itemTypeSlug, FString(TEXT("consumable")));
    TestEqual(TEXT("resolved attrs"), Drop.item.attributes.Num(), 1);

    // Miss / already-filled are no-ops.
    FDroppedItemStruct Missing = JSONParser::DeserializeDroppedItem(ParserContract_ParseObj(
        TEXT("{\"uid\":702,\"itemId\":4242,\"quantity\":1}")));
    Mgr->ResolveDropStats(Missing);
    TestTrue(TEXT("miss stays empty"), Missing.item.slug.IsEmpty());
    FDroppedItemStruct Full = JSONParser::DeserializeDroppedItem(ParserContract_ParseObj(
        TEXT("{\"uid\":703,\"itemId\":46,\"item\":{\"id\":46,\"slug\":\"server_name\"}}")));
    Mgr->ResolveDropStats(Full);
    TestEqual(TEXT("filled kept"), Full.item.slug, FString(TEXT("server_name")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVendorSlimContractTest, "MMO.Protocol.VendorSlimContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FVendorSlimContractTest::RunTest(const FString& Parameters)
{
    // S2.4 slim shop entry: dynamics only, no template keys at all.
    UVendorNetworkHandler* Handler = NewObject<UVendorNetworkHandler>(GetTransientPackage());
    TestTrue(TEXT("handler created"), Handler != nullptr);
    if (!Handler) return false;
    FVendorShopData Shop = Handler->ParseVendorShop(ParserContract_ParseObj(
        TEXT("{\"npcId\":1,\"npcSlug\":\"vendor_gor\",\"goldBalance\":500,\"items\":["
        "{\"itemId\":46,\"priceBuy\":12,\"priceSell\":4,\"stockCurrent\":7,\"stockMax\":20},"
        "{\"itemId\":2,\"priceBuy\":150,\"priceSell\":60,\"stockCurrent\":1,\"stockMax\":1}]}")));
    TestEqual(TEXT("npc kept"), Shop.npcId, 1);
    TestEqual(TEXT("two rows"), Shop.items.Num(), 2);
    if (Shop.items.Num() == 2)
    {
        TestEqual(TEXT("row item"), Shop.items[0].itemId, 46);
        TestEqual(TEXT("row price"), Shop.items[0].priceBuy, 12);
        TestEqual(TEXT("row stock"), Shop.items[0].stockCurrent, 7);
        TestTrue(TEXT("row slim: no slug"), Shop.items[0].slug.IsEmpty());
        TestEqual(TEXT("row2 stock"), Shop.items[1].stockMax, 1);
    }
    return true;
}
