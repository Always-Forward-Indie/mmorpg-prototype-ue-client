#include "Misc/AutomationTest.h"
#include "Utils/NetCore/InventoryRefreshRule.h"

// Rule for the S13-C2 post-ready refresh: the playerReady ACK proves the
// server learned frag:true, so a request issued on ACK is guaranteed
// fragmentable. The only decision is the character id guard — never ask
// for character 0 (the server rejects/ignores zero-id requests).
// Calls the production predicate directly.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReadyRefreshTest, "MMO.Protocol.ReadyRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FReadyRefreshTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("zero id never requested"), FInventoryRefreshRule::ShouldRequestOnReadyAck(0));
	TestFalse(TEXT("negative id never requested"), FInventoryRefreshRule::ShouldRequestOnReadyAck(-3));
	TestTrue(TEXT("real id requested"), FInventoryRefreshRule::ShouldRequestOnReadyAck(615));
	TestTrue(TEXT("id one requested"), FInventoryRefreshRule::ShouldRequestOnReadyAck(1));
	return true;
}
