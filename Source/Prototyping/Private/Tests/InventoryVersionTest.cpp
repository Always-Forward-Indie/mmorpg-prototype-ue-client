#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryRules.h"
#include "Utils/JSONParser.h"

// S15a inventory generation gate (chunk InventoryManager::inventoryVersions_):
// snapshots are authoritative, deltas apply only at exactly last+1, gaps
// demand an explicit full refresh. All cases call the production
// FInventoryVersionRule — no re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryVersionTest, "MMO.Protocol.InventoryVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FInventoryVersionTest::RunTest(const FString& Parameters)
{
	using V = EInventoryVersionVerdict;

	// Legacy/absent version never bricks the flow.
	TestEqual(TEXT("incoming 0 on empty baseline accepts"), FInventoryVersionRule::Classify(0, 0), V::Accept);
	TestEqual(TEXT("incoming 0 on synced baseline accepts"), FInventoryVersionRule::Classify(7, 0), V::Accept);

	// Happy path: exactly next.
	TestEqual(TEXT("first delta v1 on v0 accepts"), FInventoryVersionRule::Classify(0, 1), V::Accept);
	TestEqual(TEXT("delta v6 on v5 accepts"), FInventoryVersionRule::Classify(5, 6), V::Accept);

	// Duplicates and late/out-of-order drop silently.
	TestEqual(TEXT("same version is stale"), FInventoryVersionRule::Classify(6, 6), V::Stale);
	TestEqual(TEXT("older version is stale"), FInventoryVersionRule::Classify(6, 4), V::Stale);

	// Skipped generations demand a full refresh, never a half-merge.
	TestEqual(TEXT("delta v3 on v0 is a gap"), FInventoryVersionRule::Classify(0, 3), V::Gap);
	TestEqual(TEXT("delta v8 on v6 is a gap"), FInventoryVersionRule::Classify(6, 8), V::Gap);

	// Version parse rides the production deserializer (S15b reconnect reads
	// playerReady inventoryVersion the same way).
	{
		const FCharacterInventoryStruct WithVer = JSONParser::DeserializeCharacterInventory(
			TEXT("{\"body\":{\"characterId\":615,\"gold\":310,\"version\":7,\"items\":[]}}"));
		TestEqual(TEXT("snapshot version parsed"), WithVer.inventoryVersion, static_cast<int64>(7));
		const FCharacterInventoryStruct Legacy = JSONParser::DeserializeCharacterInventory(
			TEXT("{\"body\":{\"characterId\":615,\"gold\":310,\"items\":[]}}"));
		TestEqual(TEXT("missing version defaults to 0"), Legacy.inventoryVersion, static_cast<int64>(0));
	}

	return true;
}
