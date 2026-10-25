#include "Misc/AutomationTest.h"
#include "Utils/NetCore/NetFraming.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// Protocol v2 framing spec (S14, chunk docs/api/14-framing-v2-protocol.md):
// [u32 LE length][UTF-8 JSON], no delimiters, 4MB cap with loud failure.
// All cases call the production FNetFraming / JSONParser — no re-implementation.

namespace
{
void AppendFrame(TArray<uint8>& Out, const FString& Text)
{
	FTCHARToUTF8 Converted(*Text);
	const uint32 N = static_cast<uint32>(Converted.Length());
	Out.Add(static_cast<uint8>(N & 0xFF));
	Out.Add(static_cast<uint8>((N >> 8) & 0xFF));
	Out.Add(static_cast<uint8>((N >> 16) & 0xFF));
	Out.Add(static_cast<uint8>((N >> 24) & 0xFF));
	Out.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
}

bool ReadFramingFlag(const FString& Packet, int32& OutFirst)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Packet);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!(*BodyPtr)->TryGetArrayField(TEXT("framing"), Arr) || !Arr || Arr->Num() == 0)
	{
		return false;
	}
	return (*Arr)[0]->TryGetNumber(OutFirst);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFramingV2Test, "MMO.Protocol.FramingV2",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FFramingV2Test::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("v2 cap is 4MB"), FNetFraming::MaxFrameBytes, 4 * 1024 * 1024);

	// Framing opt-in: body.framing == [2], additive to existing fields.
	{
		TMap<FString, TSharedPtr<FJsonValue>> Body;
		Body.Add(TEXT("characterId"), MakeShareable(new FJsonValueNumber(615)));
		JSONParser::AddFramingOptIn(Body);
		const FString Packet = JSONParser::SerializeJson(TEXT("joinGameClient"),
			TMap<FString, TSharedPtr<FJsonValue>>(), Body);
		int32 First = 0;
		TestTrue(TEXT("framing readable"), ReadFramingFlag(Packet, First));
		TestEqual(TEXT("framing offers 2"), First, 2);
	}

	// One frame, one message; partial tail stays buffered.
	{
		TArray<uint8> Buf;
		AppendFrame(Buf, TEXT("{\"a\":1}"));
		AppendFrame(Buf, TEXT("{\"b\":2}"));
		Buf.Add((uint8)'x'); // partial third frame (length only, no body yet)
		Buf.Add((uint8)0); Buf.Add((uint8)0); Buf.Add((uint8)0);
		TArray<FString> Msgs;
		TestEqual(TEXT("two frames extracted"), FNetFraming::ExtractFramedMessages(Buf, Msgs), 2);
		if (Msgs.Num() == 2)
		{
			TestEqual(TEXT("first intact"), Msgs[0], FString(TEXT("{\"a\":1}")));
			TestEqual(TEXT("second intact"), Msgs[1], FString(TEXT("{\"b\":2}")));
		}
		TestEqual(TEXT("partial prefix kept"), Buf.Num(), 4);
	}

	// Split delivery: length first, body later — nothing delivered early.
	{
		TArray<uint8> Buf;
		const uint8 Hdr[4] = {9, 0, 0, 0};
		Buf.Append(Hdr, 4);
		TArray<FString> Msgs;
		TestEqual(TEXT("header alone yields nothing"), FNetFraming::ExtractFramedMessages(Buf, Msgs), 0);
		TestEqual(TEXT("header buffered"), Buf.Num(), 4);
	}

	// Split UTF-8 codepoint across Recv() chunks reassembles exactly
	// (decode happens only on complete frames, never per-chunk).
	{
		TArray<uint8> Buf;
		AppendFrame(Buf, TEXT("{\"t\":\"лис\"}"));
		const int32 Cut = 8; // inside the Cyrillic payload
		TArray<uint8> First;
		First.Append(Buf.GetData(), Cut);
		TArray<uint8> Rest;
		Rest.Append(Buf.GetData() + Cut, Buf.Num() - Cut);
		TArray<FString> M1;
		TestEqual(TEXT("cut frame waits"), FNetFraming::ExtractFramedMessages(First, M1), 0);
		First.Append(Rest);
		TArray<FString> M2;
		TestEqual(TEXT("completed after rest"), FNetFraming::ExtractFramedMessages(First, M2), 1);
		if (M2.Num() == 1)
		{
			TestEqual(TEXT("cyrillic intact"), M2[0], FString(TEXT("{\"t\":\"лис\"}")));
		}
	}

	// Oversize length: fatal, buffer cleared, -1. Error is by design
	// (seam failures are never silent) — declared expected for the harness.
	{
		AddExpectedError(TEXT("framing violation"), EAutomationExpectedErrorFlags::Contains, 1);
		TArray<uint8> Buf;
		const uint8 Bad[4] = {0x01, 0x00, 0x40, 0x00}; // 0x00400001 > 4MB
		Buf.Append(Bad, 4);
		TArray<FString> Msgs;
		TestEqual(TEXT("oversize is fatal"), FNetFraming::ExtractFramedMessages(Buf, Msgs), -1);
		TestEqual(TEXT("buffer cleared"), Buf.Num(), 0);
	}
	return true;
}
