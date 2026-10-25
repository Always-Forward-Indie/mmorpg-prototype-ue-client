#include "Misc/AutomationTest.h"
#include "Utils/NetCore/WireFragReassembler.h"

// S13-C3: reassembly observability transitions. Success must be as loud
// as failure — the worker feeds this pure struct, the spec pins it.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireFragStatsTest, "MMO.Protocol.WireFragStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FWireFragStatsTest::RunTest(const FString& Parameters)
{
	FWireFragStats S;
	TestEqual(TEXT("fresh frames zero"), S.FramesSeen, (uint64)0);
	TestEqual(TEXT("fresh streams zero"), S.StreamsCompleted, (uint64)0);
	TestEqual(TEXT("fresh malformed zero"), S.FramesMalformed, (uint64)0);
	TestEqual(TEXT("fresh bytes zero"), S.BytesReassembled, (uint64)0);

	// Two-frame stream: two fed, one completion, bytes summed once.
	S.OnFrameFed();
	S.OnFrameFed();
	S.OnStreamCompleted(9000);
	TestEqual(TEXT("two frames fed"), S.FramesSeen, (uint64)2);
	TestEqual(TEXT("one stream done"), S.StreamsCompleted, (uint64)1);
	TestEqual(TEXT("bytes summed"), S.BytesReassembled, (uint64)9000);
	TestEqual(TEXT("no malformed"), S.FramesMalformed, (uint64)0);

	// Malformed frames never touch the completion counters.
	S.OnFrameMalformed();
	TestEqual(TEXT("one malformed"), S.FramesMalformed, (uint64)1);
	TestEqual(TEXT("streams unchanged"), S.StreamsCompleted, (uint64)1);
	return true;
}
