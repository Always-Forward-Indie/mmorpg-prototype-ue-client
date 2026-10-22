#include "Misc/AutomationTest.h"
#include "Utils/NetCore/NetFraming.h"
#include "Utils/NetCore/WireFragReassembler.h"

// Spec (server twin: chunk tests/test_json_chunking.cpp, WireFrag cases):
// wireFrag frames group by streamId; order is free, duplicates ignored,
// mismatched totals reset the stream; limits 1MB / 1024 frames / 16 streams
// / 30s reap. Reassembly is byte-exact (server slices by bytes, UTF-8 may
// straddle frames) and generic — no per-event special cases.
// All cases below call the production FNetFraming / FWireFragReassembler.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireFragReassemblerTest, "MMO.Protocol.WireFrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
void AppendLine(TArray<uint8>& Out, const FString& Text)
{
	FTCHARToUTF8 Converted(*Text);
	Out.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	Out.Add((uint8)'\n');
}

// Hand-built wireFrag frame line (mirrors server packWireMessage layout).
FString MakeFrame(uint64 StreamId, uint64 Index, uint64 Total, const FString& Payload)
{
	return FString::Printf(TEXT("{\"header\":{\"eventType\":\"wireFrag\",\"status\":\"pending\",\"clientId\":0,\"hash\":\"\"},\"body\":{\"streamId\":%llu,\"index\":%llu,\"total\":%llu,\"payload\":\"%s\"}}"),
		StreamId, Index, Total, *Payload.Replace(TEXT("\""), TEXT("\\\"")));
}

// Parse one frame line and feed its payload. Returns false when the line
// is not a well-formed frame (caller swallows it).
bool FeedFrameLine(FWireFragReassembler& R, const FString& Line, double NowSec,
	FWireFragReassembler::FFeedResult& Out)
{
	FTCHARToUTF8 Raw(*Line);
	const int32 Len = Raw.Length();
	FNetFraming::FWireFragFramePos Pos;
	if (!FNetFraming::ParseWireFragFrame(reinterpret_cast<const uint8*>(Raw.Get()), Len, Pos))
	{
		return false;
	}
	TArray<uint8> Payload;
	if (!FNetFraming::UnescapeJsonString(reinterpret_cast<const uint8*>(Raw.Get()) + Pos.PayloadBegin, Pos.PayloadLen, Payload))
	{
		return false;
	}
	Out = R.Feed(Pos.StreamId, Pos.Index, Pos.Total,
		Payload.Num() > 0 ? Payload.GetData() : nullptr, Payload.Num(), NowSec);
	return true;
}

FString BytesToFString(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() == 0)
	{
		return FString();
	}
	FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	return FString(Conv.Length(), Conv.Get());
}
}

bool FWireFragReassemblerTest::RunTest(const FString& Parameters)
{
	// Contract constants match the server twin.
	TestEqual(TEXT("frag budget under cap"), FWireFragReassembler::MaxPayloadBytes - 1024, 6 * 1024);
	TestEqual(TEXT("logical cap 1MB"), FWireFragReassembler::MaxLogicalBytes, 1024 * 1024);
	TestEqual(TEXT("max frames 1024"), FWireFragReassembler::MaxFramesPerMessage, (uint64)1024);
	TestEqual(TEXT("max streams 16"), FWireFragReassembler::MaxStreams, 16);
	TestEqual(TEXT("reap ttl 30s"), FWireFragReassembler::DefaultReapTtlSec, 30.0);

	// Frame parse: fields, 0-based index, payload span with an escaped quote.
	{
		FString Line = MakeFrame(42, 1, 3, TEXT("ab\"cd"));
		FTCHARToUTF8 Raw(*Line);
		FNetFraming::FWireFragFramePos Pos;
		TestTrue(TEXT("frame parses"), FNetFraming::ParseWireFragFrame(
			reinterpret_cast<const uint8*>(Raw.Get()), Raw.Length(), Pos));
		TestEqual(TEXT("stream id"), Pos.StreamId, (uint64)42);
		TestEqual(TEXT("index 0-based"), Pos.Index, (uint64)1);
		TestEqual(TEXT("total"), Pos.Total, (uint64)3);
		TestEqual(TEXT("payload raw bytes"), Pos.PayloadLen, 6); // ab\"cd on the wire
	}
	// Payload quoting must not confuse the number scan: quoted "total":999
	// inside the payload is ignored, the real total wins.
	{
		FString Line = TEXT("{\"header\":{\"eventType\":\"wireFrag\"},\"body\":{\"streamId\":7,\"index\":0,\"total\":2,\"payload\":\"{\\\"total\\\":999}\"}}");
		FTCHARToUTF8 Raw(*Line);
		FNetFraming::FWireFragFramePos Pos;
		TestTrue(TEXT("numbers before payload win"), FNetFraming::ParseWireFragFrame(
			reinterpret_cast<const uint8*>(Raw.Get()), Raw.Length(), Pos));
		TestEqual(TEXT("real total kept"), Pos.Total, (uint64)2);
	}
	// Live order-independence: the deployed sender emits keys
	// alphabetically (body before header, numbers after payload).
	{
		FString Line = TEXT("{\"body\":{\"index\":1,\"payload\":\"CD\",\"streamId\":9,\"total\":2},\"header\":{\"clientId\":0,\"eventType\":\"wireFrag\",\"hash\":\"\",\"status\":\"pending\"}}");
		FTCHARToUTF8 Raw(*Line);
		FNetFraming::FWireFragFramePos Pos;
		TestTrue(TEXT("alpha order parses"), FNetFraming::ParseWireFragFrame(
			reinterpret_cast<const uint8*>(Raw.Get()), Raw.Length(), Pos));
		if (Pos.PayloadLen > 0)
		{
			TestEqual(TEXT("alpha stream id"), Pos.StreamId, (uint64)9);
			TestEqual(TEXT("alpha index"), Pos.Index, (uint64)1);
			TestEqual(TEXT("alpha total"), Pos.Total, (uint64)2);
			FWireFragReassembler R;
			FWireFragReassembler::FFeedResult L;
			TArray<uint8> Payload;
			TestTrue(TEXT("alpha payload unescapes"), FNetFraming::UnescapeJsonString(
				reinterpret_cast<const uint8*>(Raw.Get()) + Pos.PayloadBegin, Pos.PayloadLen, Payload));
			L = R.Feed(Pos.StreamId, Pos.Index, Pos.Total,
				Payload.Num() > 0 ? Payload.GetData() : nullptr, Payload.Num(), 150.0);
			TestFalse(TEXT("alpha first incomplete"), L.bComplete);
			TestEqual(TEXT("alpha stream pending"), R.PendingStreams(), 1);
		}
	}
	// Malformed frames rejected.
	{
		FNetFraming::FWireFragFramePos Pos;
		FTCHARToUTF8 A(*FString(TEXT("{\"body\":{\"streamId\":1,\"index\":0}}")));
		TestFalse(TEXT("missing total rejected"), FNetFraming::ParseWireFragFrame(
			reinterpret_cast<const uint8*>(A.Get()), A.Length(), Pos));
		FTCHARToUTF8 B(*FString(TEXT("{\"body\":{\"streamId\":\"x\",\"index\":0,\"total\":1,\"payload\":\"a\"}}")));
		TestFalse(TEXT("non-numeric rejected"), FNetFraming::ParseWireFragFrame(
			reinterpret_cast<const uint8*>(B.Get()), B.Length(), Pos));
		TestFalse(TEXT("empty rejected"), FNetFraming::ParseWireFragFrame(nullptr, 0, Pos));
	}

	// In-order assembly, byte-exact.
	{
		FWireFragReassembler R;
		FWireFragReassembler::FFeedResult Last;
		TestTrue(TEXT("frame 0 feeds"), FeedFrameLine(R, MakeFrame(100, 0, 2, TEXT("{\"a\":")) , 100.0, Last));
		TestFalse(TEXT("frame 0 incomplete"), Last.bComplete);
		TestTrue(TEXT("frame 1 feeds"), FeedFrameLine(R, MakeFrame(100, 1, 2, TEXT("1}")), 100.0, Last));
		TestTrue(TEXT("completed"), Last.bComplete);
		TestFalse(TEXT("not dropped"), Last.bDropped);
		TestEqual(TEXT("message byte-exact"), BytesToFString(Last.Message), FString(TEXT("{\"a\":1}")));
		TestEqual(TEXT("stream released"), R.PendingStreams(), 0);
	}
	// Out-of-order + duplicate tolerance; a duplicate after completion opens
	// a fresh incomplete stream (never fabricates data).
	{
		FWireFragReassembler R;
		FWireFragReassembler::FFeedResult Last;
		TestTrue(TEXT("index 1 first"), FeedFrameLine(R, MakeFrame(101, 1, 2, TEXT("CD")), 200.0, Last));
		TestFalse(TEXT("incomplete"), Last.bComplete);
		TestTrue(TEXT("index 0 completes"), FeedFrameLine(R, MakeFrame(101, 0, 2, TEXT("AB")), 200.0, Last));
		TestTrue(TEXT("completed reversed"), Last.bComplete);
		TestEqual(TEXT("reversed bytes in order"), BytesToFString(Last.Message), FString(TEXT("ABCD")));
		TestTrue(TEXT("late duplicate feeds"), FeedFrameLine(R, MakeFrame(101, 1, 2, TEXT("CD")), 200.0, Last));
		TestFalse(TEXT("no phantom complete"), Last.bComplete);
		TestEqual(TEXT("one pending stream"), R.PendingStreams(), 1);
		TestEqual(TEXT("reap clears"), R.ReapIdle(201.0, 0.0), 0);
	}
	// Malformed feeds mirror the server twin (erase + dropped).
	{
		FWireFragReassembler R;
		FWireFragReassembler::FFeedResult L;
		uint8 X = 'x';
		L = R.Feed(1, 0, 0, &X, 1, 300.0);
		TestTrue(TEXT("total 0 dropped"), L.bDropped);
		L = R.Feed(2, 5, 3, &X, 1, 300.0);
		TestTrue(TEXT("index>=total dropped"), L.bDropped);
		L = R.Feed(3, 0, 2, &X, 1, 300.0);
		TestFalse(TEXT("first ok"), L.bDropped || L.bComplete);
		TestEqual(TEXT("one pending"), R.PendingStreams(), 1);
		L = R.Feed(3, 1, 3, &X, 1, 300.0);
		TestTrue(TEXT("total mismatch dropped"), L.bDropped);
		TestEqual(TEXT("stream reset"), R.PendingStreams(), 0);
		uint8 C = 'C';
		L = R.Feed(4, 1, 2, &C, 1, 300.0);
		uint8 D = 'D';
		L = R.Feed(4, 0, 2, &D, 1, 300.0);
		TestTrue(TEXT("out-of-order completes"), L.bComplete);
		TestEqual(TEXT("message has both"), L.Message.Num(), 2);
	}
	// Stream cap: 16 accepted, 17th dropped; reap clears.
	{
		FWireFragReassembler R;
		FWireFragReassembler::FFeedResult L;
		uint8 X = 'a';
		for (uint64 s = 1; s <= 18; ++s)
		{
			L = R.Feed(s, 0, 2, &X, 1, 400.0);
			if (s <= 16)
			{
				TestFalse(TEXT("within cap kept"), L.bDropped);
			}
			else
			{
				TestTrue(TEXT("over cap dropped"), L.bDropped);
			}
		}
		TestEqual(TEXT("16 pending"), R.PendingStreams(), 16);
		TestEqual(TEXT("reap clears all"), R.ReapIdle(401.0, 0.0), 0);
	}
	// 30s reap: kept at +29, dropped past +30.
	{
		FWireFragReassembler R;
		FWireFragReassembler::FFeedResult L;
		uint8 X = 'a';
		R.Feed(9, 0, 2, &X, 1, 500.0);
		TestEqual(TEXT("kept at +29"), R.ReapIdle(529.0), 1);
		TestEqual(TEXT("dropped past +30"), R.ReapIdle(531.0), 0);
	}
	// UTF-8 split across frames round-trips (server emits split codepoints).
	{
		FWireFragReassembler R;
		FWireFragReassembler::FFeedResult L;
		FTCHARToUTF8 Full(*FString(TEXT("лис")));
		const uint8* B = reinterpret_cast<const uint8*>(Full.Get());
		const int32 N = Full.Length(); // D0 BB D0 B8 D1 81
		TestTrue(TEXT("cyrillic is multibyte"), N > 3);
		const int32 Cut = 1; // split inside the first codepoint
		L = R.Feed(11, 0, 2, B, Cut, 600.0);
		TestFalse(TEXT("first half incomplete"), L.bComplete);
		L = R.Feed(11, 1, 2, B + Cut, N - Cut, 600.0);
		TestTrue(TEXT("split codepoint completes"), L.bComplete);
		TestEqual(TEXT("cyrillic round-trips"), BytesToFString(L.Message), FString(TEXT("лис")));
	}
	// Unescape: quotes, backslashes, controls, \uXXXX incl. Cyrillic.
	// Input a\"b\\c\n\u043B decodes to a " b \ c \n л (8 bytes).
	{
		TArray<uint8> Out;
		FTCHARToUTF8 A(*FString(TEXT("a\\\"b\\\\c\\n\\u043B")));
		TestTrue(TEXT("escapes decode"), FNetFraming::UnescapeJsonString(
			reinterpret_cast<const uint8*>(A.Get()), A.Length(), Out));
		TestEqual(TEXT("unescaped size"), Out.Num(), 8);
		TestEqual(TEXT("unescaped text"), BytesToFString(Out), FString(TEXT("a\"b\\c\nл")));
		FTCHARToUTF8 Bad(*FString(TEXT("ab\\")));
		TestFalse(TEXT("truncated escape rejected"), FNetFraming::UnescapeJsonString(
			reinterpret_cast<const uint8*>(Bad.Get()), Bad.Length(), Out));
		FTCHARToUTF8 BadU(*FString(TEXT("\\uZZZZ")));
		TestFalse(TEXT("bad hex rejected"), FNetFraming::UnescapeJsonString(
			reinterpret_cast<const uint8*>(BadU.Get()), BadU.Length(), Out));
	}
	// Generic reassembly: a big mobMoveUpdate-shaped message (not only
	// inventory) splits and rejoins byte-exact.
	{
		FString Big = TEXT("{\"header\":{\"eventType\":\"mobMoveUpdate\"},\"body\":{\"mobs\":[");
		for (int32 i = 0; i < 300; ++i)
		{
			Big += FString::Printf(TEXT("{\"uid\":%d,\"x\":%.1f,\"y\":%.1f},"), 1000 + i, 1.5 * i, 2.5 * i);
		}
		Big += TEXT("]}}");
		FTCHARToUTF8 Raw(*Big);
		const int32 Total = (Raw.Length() + 6143) / 6144;
		TestTrue(TEXT("message oversize"), Total > 1);
		FWireFragReassembler R;
		FWireFragReassembler::FFeedResult L;
		const uint8* B = reinterpret_cast<const uint8*>(Raw.Get());
		for (int32 i = Total - 1; i >= 0; --i)
		{
			const int32 Begin = i * 6144;
			const int32 Len = FMath::Min(6144, Raw.Length() - Begin);
			L = R.Feed(21, (uint64)i, (uint64)Total, B + Begin, Len, 700.0);
		}
		TestTrue(TEXT("generic message completes"), L.bComplete);
		TestEqual(TEXT("generic byte-exact"), BytesToFString(L.Message), Big);
	}
	// Raw-line extraction keeps wireFrag bytes intact for the reassembler.
	{
		TArray<uint8> Buf;
		AppendLine(Buf, TEXT("{\"a\":1}"));
		AppendLine(Buf, MakeFrame(5, 0, 1, TEXT("xy")));
		TArray<TArray<uint8>> Lines;
		TestEqual(TEXT("two raw lines"), FNetFraming::ExtractRawLines(Buf, Lines), 2);
		TestEqual(TEXT("buffer drained"), Buf.Num(), 0);
		if (Lines.Num() == 2)
		{
			TestEqual(TEXT("frame type peek"),
				FNetFraming::PeekEventType(Lines[1].GetData(), Lines[1].Num()), FString(TEXT("wireFrag")));
			TestEqual(TEXT("normal type peek"),
				FNetFraming::PeekEventType(Lines[0].GetData(), Lines[0].Num()), FString(TEXT("?")));
		}
	}

	return true;
}
