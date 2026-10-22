#include "Utils/NetCore/WireFragReassembler.h"
#include "Utils/NetCore/NetFraming.h"
#include "Prototyping.h"

FWireFragReassembler::FFeedResult FWireFragReassembler::Feed(uint64 StreamId, uint64 Index, uint64 Total,
	const uint8* Payload, int32 PayloadLen, double NowSec)
{
	FFeedResult Result;
	if (Total == 0 || Total > MaxFramesPerMessage || Index >= Total
		|| PayloadLen < 0 || PayloadLen > MaxPayloadBytes
		|| (PayloadLen > 0 && Payload == nullptr))
	{
		Streams.Remove(StreamId);
		Result.bDropped = true;
		return Result;
	}
	FStream* Found = Streams.Find(StreamId);
	if (Found == nullptr)
	{
		if (Streams.Num() >= MaxStreams)
		{
			Result.bDropped = true;
			return Result;
		}
		FStream New;
		New.Total = Total;
		New.Parts.SetNum(static_cast<int32>(Total));
		New.Filled.SetNumZeroed(static_cast<int32>(Total));
		Found = &Streams.Add(StreamId, MoveTemp(New));
	}
	else if (Found->Total != Total)
	{
		Streams.Remove(StreamId);
		Result.bDropped = true;
		return Result;
	}
	FStream& S = *Found;
	const int32 Slot = static_cast<int32>(Index);
	if (S.Filled[Slot] == 0)
	{
		S.Filled[Slot] = 1;
		++S.Have;
	}
	S.Parts[Slot].SetNumUninitialized(PayloadLen);
	if (PayloadLen > 0)
	{
		FMemory::Memcpy(S.Parts[Slot].GetData(), Payload, PayloadLen);
	}
	S.LastUpdateSec = NowSec;
	if (S.Have == static_cast<int32>(S.Total))
	{
		int32 Bytes = 0;
		for (const TArray<uint8>& Part : S.Parts)
		{
			Bytes += Part.Num();
		}
		Result.Message.SetNumUninitialized(Bytes);
		int32 At = 0;
		for (const TArray<uint8>& Part : S.Parts)
		{
			if (Part.Num() > 0)
			{
				FMemory::Memcpy(Result.Message.GetData() + At, Part.GetData(), Part.Num());
				At += Part.Num();
			}
		}
		Streams.Remove(StreamId);
		Result.bComplete = true;
	}
	return Result;
}

int32 FWireFragReassembler::ReapIdle(double NowSec, double TtlSec)
{
	TArray<uint64> Stale;
	for (const auto& Pair : Streams)
	{
		if (NowSec - Pair.Value.LastUpdateSec > TtlSec)
		{
			Stale.Add(Pair.Key);
		}
	}
	for (const uint64 Key : Stale)
	{
		Streams.Remove(Key);
	}
	return Streams.Num();
}

namespace
{
// Byte-literal key match at Pos (no decoding — keys are pure ASCII).
bool MatchKey(const uint8* Data, int32 Len, int32 Pos, const ANSICHAR* Key, int32 KeyLen)
{
	if (Pos < 0 || Pos + KeyLen > Len)
	{
		return false;
	}
	for (int32 k = 0; k < KeyLen; ++k)
	{
		if (Data[Pos + k] != static_cast<uint8>(Key[k]))
		{
			return false;
		}
	}
	return true;
}

// Find `"Key"` then ':' then an unsigned decimal, with the key's opening
// quote constrained to the allowed zones (payload value bytes are
// excluded — a payload may quote "total":N text of its own, and live
// senders emit number keys after the payload).
// Strict digits only (no sign, no fraction, no exponent) — the sender
// emits uint64 fields; anything else is a shape violation.
bool FindJsonUintZoned(const uint8* Data, int32 Len, const ANSICHAR* Key, uint64& OutVal,
	int32 Zone0Begin, int32 Zone0End, int32 Zone1Begin, int32 Zone1End)
{
	int32 KeyLen = 0;
	while (Key[KeyLen] != '\0')
	{
		++KeyLen;
	}
	auto InZones = [=](int32 Pos) -> bool
	{
		return (Pos >= Zone0Begin && Pos < Zone0End) || (Pos >= Zone1Begin && Pos < Zone1End);
	};
	for (int32 i = 0; i + KeyLen + 2 < Len; ++i)
	{
		if (!InZones(i))
		{
			continue;
		}
		if (Data[i] != '"')
		{
			continue;
		}
		if (!MatchKey(Data, Len, i + 1, Key, KeyLen) || Data[i + 1 + KeyLen] != '"')
		{
			continue;
		}
		int32 j = i + 1 + KeyLen + 1;
		while (j < Len && (Data[j] == ' ' || Data[j] == '\t'))
		{
			++j;
		}
		if (j >= Len || Data[j] != ':')
		{
			continue;
		}
		++j;
		while (j < Len && (Data[j] == ' ' || Data[j] == '\t'))
		{
			++j;
		}
		if (j >= Len || Data[j] < '0' || Data[j] > '9')
		{
			return false;
		}
		uint64 Val = 0;
		while (j < Len && Data[j] >= '0' && Data[j] <= '9')
		{
			const uint64 Digit = static_cast<uint64>(Data[j] - '0');
			if (Val > (MAX_uint64 - Digit) / 10)
			{
				return false;
			}
			Val = Val * 10 + Digit;
			++j;
		}
		OutVal = Val;
		return true;
	}
	return false;
}
} // namespace

bool FNetFraming::ParseWireFragFrame(const uint8* Data, int32 Len, FWireFragFramePos& OutPos)
{
	OutPos = FWireFragFramePos();
	if (!Data || Len <= 0)
	{
		return false;
	}
	// Locate the payload value span first: number fields are trusted
	// only OUTSIDE of it (a payload may quote "total":N text of its own,
	// and live senders emit number keys after the payload).
	int32 PayloadBegin = -1;
	int32 PayloadEnd = -1; // one past the closing quote
	{
		static const ANSICHAR PKey[] = "\"payload\"";
		static constexpr int32 PKeyLen = 9; // strlen("\"payload\"")
		int32 PayloadKeyAt = -1;
		for (int32 i = 0; i + PKeyLen <= Len; ++i)
		{
			if (MatchKey(Data, Len, i, PKey, PKeyLen))
			{
				PayloadKeyAt = i;
				break;
			}
		}
		if (PayloadKeyAt >= 0)
		{
			// Payload value: opening quote after "payload" ':', closing
			// unescaped quote. The sender escapes '"' ('\\"') and controls,
			// so raw UTF-8 bytes (including split codepoints) pass through
			// untouched.
			int32 j = PayloadKeyAt + PKeyLen;
			while (j < Len && (Data[j] == ' ' || Data[j] == '\t'))
			{
				++j;
			}
			if (j < Len && Data[j] == ':')
			{
				++j;
				while (j < Len && (Data[j] == ' ' || Data[j] == '\t'))
				{
					++j;
				}
				if (j < Len && Data[j] == '"')
				{
					++j;
					PayloadBegin = j;
					while (j < Len)
					{
						if (Data[j] == '\\')
						{
							j += 2;
							continue;
						}
						if (Data[j] == '"')
						{
							break;
						}
						++j;
					}
					if (j < Len)
					{
						PayloadEnd = j + 1;
					}
				}
			}
		}
	}
	if (PayloadBegin < 0 || PayloadEnd < 0)
	{
		return false;
	}
	uint64 StreamId = 0;
	uint64 Index = 0;
	uint64 Total = 0;
	if (!FindJsonUintZoned(Data, Len, "streamId", StreamId, 0, PayloadBegin, PayloadEnd, Len)
		|| !FindJsonUintZoned(Data, Len, "index", Index, 0, PayloadBegin, PayloadEnd, Len)
		|| !FindJsonUintZoned(Data, Len, "total", Total, 0, PayloadBegin, PayloadEnd, Len))
	{
		return false;
	}
	OutPos.StreamId = StreamId;
	OutPos.Index = Index;
	OutPos.Total = Total;
	OutPos.PayloadBegin = PayloadBegin;
	OutPos.PayloadLen = PayloadEnd - 1 - PayloadBegin;
	return true;
}

bool FNetFraming::UnescapeJsonString(const uint8* Data, int32 Len, TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	if (!Data && Len > 0)
	{
		return false;
	}
	auto AppendUtf8 = [&OutBytes](uint32 Codepoint)
	{
		if (Codepoint < 0x80)
		{
			OutBytes.Add(static_cast<uint8>(Codepoint));
		}
		else if (Codepoint < 0x800)
		{
			OutBytes.Add(static_cast<uint8>(0xC0 | (Codepoint >> 6)));
			OutBytes.Add(static_cast<uint8>(0x80 | (Codepoint & 0x3F)));
		}
		else if (Codepoint < 0x10000)
		{
			OutBytes.Add(static_cast<uint8>(0xE0 | (Codepoint >> 12)));
			OutBytes.Add(static_cast<uint8>(0x80 | ((Codepoint >> 6) & 0x3F)));
			OutBytes.Add(static_cast<uint8>(0x80 | (Codepoint & 0x3F)));
		}
		else if (Codepoint <= 0x10FFFF)
		{
			OutBytes.Add(static_cast<uint8>(0xF0 | (Codepoint >> 18)));
			OutBytes.Add(static_cast<uint8>(0x80 | ((Codepoint >> 12) & 0x3F)));
			OutBytes.Add(static_cast<uint8>(0x80 | ((Codepoint >> 6) & 0x3F)));
			OutBytes.Add(static_cast<uint8>(0x80 | (Codepoint & 0x3F)));
		}
		else
		{
			return false;
		}
		return true;
	};
	auto HexVal = [](uint8 C) -> int32
	{
		if (C >= '0' && C <= '9')
		{
			return C - '0';
		}
		if (C >= 'a' && C <= 'f')
		{
			return C - 'a' + 10;
		}
		if (C >= 'A' && C <= 'F')
		{
			return C - 'A' + 10;
		}
		return -1;
	};
	for (int32 i = 0; i < Len; ++i)
	{
		const uint8 C = Data[i];
		if (C != '\\')
		{
			OutBytes.Add(C);
			continue;
		}
		if (i + 1 >= Len)
		{
			return false;
		}
		const uint8 E = Data[++i];
		switch (E)
		{
		case '"': OutBytes.Add('"'); break;
		case '\\': OutBytes.Add('\\'); break;
		case '/': OutBytes.Add('/'); break;
		case 'b': OutBytes.Add('\b'); break;
		case 'f': OutBytes.Add('\f'); break;
		case 'n': OutBytes.Add('\n'); break;
		case 'r': OutBytes.Add('\r'); break;
		case 't': OutBytes.Add('\t'); break;
		case 'u':
		{
			if (i + 4 >= Len)
			{
				return false;
			}
			uint32 Cp = 0;
			for (int32 k = 1; k <= 4; ++k)
			{
				const int32 H = HexVal(Data[i + k]);
				if (H < 0)
				{
					return false;
				}
				Cp = (Cp << 4) | static_cast<uint32>(H);
			}
			i += 4;
			if (Cp >= 0xD800 && Cp <= 0xDBFF)
			{
				// High surrogate: expect \uDC00..\uDFFF.
				if (i + 6 > Len || Data[i + 1] != '\\' || Data[i + 2] != 'u')
				{
					return false;
				}
				uint32 Lo = 0;
				for (int32 k = 3; k <= 6; ++k)
				{
					const int32 H = HexVal(Data[i + k]);
					if (H < 0)
					{
						return false;
					}
					Lo = (Lo << 4) | static_cast<uint32>(H);
				}
				if (Lo < 0xDC00 || Lo > 0xDFFF)
				{
					return false;
				}
				Cp = 0x10000 + ((Cp - 0xD800) << 10) + (Lo - 0xDC00);
				i += 6;
			}
			else if (Cp >= 0xDC00 && Cp <= 0xDFFF)
			{
				return false;
			}
			if (!AppendUtf8(Cp))
			{
				return false;
			}
			break;
		}
		default:
			return false;
		}
	}
	return true;
}
