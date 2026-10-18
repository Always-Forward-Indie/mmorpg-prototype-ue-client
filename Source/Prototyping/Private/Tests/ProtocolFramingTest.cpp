#include "Misc/AutomationTest.h"
#include "Utils/NetCore/NetFraming.h"

// Spec (Documentation/Server Info/API/00-protocol-overview.md framing rules):
// one JSON object per line, '\n' terminated, empty lines ignored, 8KB max.
// All cases below call the production FNetFraming — no local re-implementation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtocolFramingTest, "MMO.Protocol.Framing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
void AppendUtf8(TArray<uint8>& OutBytes, const FString& Text)
{
	FTCHARToUTF8 Converted(*Text);
	OutBytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
}
}

bool FProtocolFramingTest::RunTest(const FString& Parameters)
{
	// FString-level split: empty lines ignored, trailing partial kept as rest.
	TArray<FString> Lines;
	FString Rest;
	TestEqual(TEXT("two messages parsed"),
		FNetFraming::SplitFraming(TEXT("\n{\"a\":1}\n\n{\"b\":2}\n{\"par"), Lines, Rest), 2);
	TestEqual(TEXT("partial rest kept"), Rest, FString(TEXT("{\"par")));

	// 8KB cap from the spec.
	TestEqual(TEXT("8KB cap is 8192"), FNetFraming::MaxMessageBytes, 8 * 1024);
	TestFalse(TEXT("cap-size ok"), FNetFraming::IsOversized(FNetFraming::MaxMessageBytes));
	TestTrue(TEXT("cap+1 oversized"), FNetFraming::IsOversized(FNetFraming::MaxMessageBytes + 1));

	// CRLF tolerance: trailing '\r' trimmed.
	TArray<FString> CrlfLines;
	FString CrlfRest;
	TestEqual(TEXT("crlf message parsed"),
		FNetFraming::SplitFraming(TEXT("{\"a\":1}\r\n"), CrlfLines, CrlfRest), 1);
	if (CrlfLines.Num() == 1)
	{
		TestEqual(TEXT("cr trimmed"), CrlfLines[0], FString(TEXT("{\"a\":1}")));
	}

	// Empty-only buffer: no messages, empty rest.
	TArray<FString> EmptyLines;
	FString EmptyRest;
	TestEqual(TEXT("empty buffer parses to zero"),
		FNetFraming::SplitFraming(TEXT("\n\n"), EmptyLines, EmptyRest), 0);

	// Byte-level extractor (production path used by NetworkReceiverWorker):
	// split across chunks, empty lines ignored, partial tail stays buffered.
	TArray<uint8> Accumulated;
	AppendUtf8(Accumulated, TEXT("{\"a\":1}\n\n{\"b\":2}\n{\"par"));
	TArray<FString> Packets;
	TestEqual(TEXT("two packets extracted"), FNetFraming::ExtractPackets(Accumulated, Packets), 2);
	if (Packets.Num() == 2)
	{
		TestEqual(TEXT("first packet"), Packets[0], FString(TEXT("{\"a\":1}")));
		TestEqual(TEXT("second packet"), Packets[1], FString(TEXT("{\"b\":2}")));
	}
	FTCHARToUTF8 ExpectedRest(*FString(TEXT("{\"par")));
	TestEqual(TEXT("partial tail stays buffered"), Accumulated.Num(), ExpectedRest.Length());

	// Oversized segment dropped, following valid packet still delivered.
	// The drop logs at Error level by design (seam failures are never
	// silent), so the test must declare it expected for the harness.
	AddExpectedError(TEXT("dropping oversized"), EAutomationExpectedErrorFlags::Contains, 1);
	TArray<uint8> Oversized;
	for (int32 i = 0; i < FNetFraming::MaxMessageBytes + 1; ++i)
	{
		Oversized.Add((uint8)'x');
	}
	AppendUtf8(Oversized, TEXT("\n{\"ok\":1}\n"));
	TArray<FString> AfterDrop;
	TestEqual(TEXT("oversized dropped, valid kept"), FNetFraming::ExtractPackets(Oversized, AfterDrop), 1);
	if (AfterDrop.Num() == 1)
	{
		TestEqual(TEXT("valid packet intact"), AfterDrop[0], FString(TEXT("{\"ok\":1}")));
	}
	TestEqual(TEXT("buffer drained"), Oversized.Num(), 0);

	// Event-type peek for drop logs (byte-level, no decoding — safe on garbage).
	auto Peek = [](const FString& Text)
	{
		FTCHARToUTF8 Converted(*Text);
		return FNetFraming::PeekEventType(
			reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	};
	TestEqual(TEXT("peek finds type"), Peek(TEXT("{\"header\":{\"eventType\":\"mobMoveUpdate\"},\"body\":{}}")),
		FString(TEXT("mobMoveUpdate")));
	TestEqual(TEXT("peek spaced type"), Peek(TEXT("{\"header\" : {\"eventType\" : \"spawnMobsInZone\"}}")),
		FString(TEXT("spawnMobsInZone")));
	TestEqual(TEXT("peek missing is ?"), Peek(TEXT("{\"header\":{}}")), FString(TEXT("?")));
	TestEqual(TEXT("peek garbage is ?"), Peek(TEXT("not json at all")), FString(TEXT("?")));
	TestEqual(TEXT("peek empty is ?"), FNetFraming::PeekEventType(nullptr, 0), FString(TEXT("?")));
	TestEqual(TEXT("peek head sanitizes"), FNetFraming::PeekHead((const uint8*)"ab\x01\x7F" "cd", 6),
		FString(TEXT("ab??cd")));

	return true;
}
