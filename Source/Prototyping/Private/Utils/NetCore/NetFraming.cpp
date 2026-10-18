#include "Utils/NetCore/NetFraming.h"
#include "Prototyping.h"

int32 FNetFraming::SplitFraming(const FString& InBuffer, TArray<FString>& OutMessages, FString& OutRest)
{
	OutMessages.Reset();
	TArray<FString> Parts;
	InBuffer.ParseIntoArray(Parts, TEXT("\n"), /*InCullEmpty=*/false);
	OutRest = Parts.Num() > 0 ? Parts.Pop() : FString();
	for (const FString& Part : Parts)
	{
		FString Trimmed = Part.TrimStartAndEnd();
		if (!Trimmed.IsEmpty())
		{
			OutMessages.Add(Trimmed);
		}
	}
	return OutMessages.Num();
}

int32 FNetFraming::ExtractPackets(TArray<uint8>& InOutAccumulatedBytes, TArray<FString>& OutMessages,
const TCHAR* SocketTag)
{
	OutMessages.Reset();

	int32 DelimiterIndex = INDEX_NONE;
	while ((DelimiterIndex = InOutAccumulatedBytes.Find((uint8)'\n')) != INDEX_NONE)
	{
		if (DelimiterIndex > MaxMessageBytes)
		{
			UE_LOG(LogNetPacket, Error, TEXT("FNetFraming: dropping oversized %d-byte packet (cap %d) socket=%s eventType=%s head=%s"),
				DelimiterIndex, MaxMessageBytes, SocketTag ? SocketTag : TEXT("?"),
				*PeekEventType(InOutAccumulatedBytes.GetData(), DelimiterIndex),
				*PeekHead(InOutAccumulatedBytes.GetData(), DelimiterIndex));
			InOutAccumulatedBytes.RemoveAt(0, DelimiterIndex + 1);
			continue;
		}
		if (DelimiterIndex == 0)
		{
			// Empty line: ignored per spec, never forwarded downstream.
			InOutAccumulatedBytes.RemoveAt(0, 1);
			continue;
		}

		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(InOutAccumulatedBytes.GetData()), DelimiterIndex);
		FString Message(Converter.Length(), Converter.Get());
		InOutAccumulatedBytes.RemoveAt(0, DelimiterIndex + 1);

		Message.TrimStartAndEndInline();
		if (Message.IsEmpty())
		{
			continue;
		}
		OutMessages.Add(MoveTemp(Message));
	}

	if (InOutAccumulatedBytes.Num() > MaxMessageBytes)
	{
		UE_LOG(LogNetPacket, Error, TEXT("FNetFraming: dropping %d-byte partial without delimiter (cap %d) socket=%s eventType=%s head=%s"),
			InOutAccumulatedBytes.Num(), MaxMessageBytes, SocketTag ? SocketTag : TEXT("?"),
			*PeekEventType(InOutAccumulatedBytes.GetData(), InOutAccumulatedBytes.Num()),
			*PeekHead(InOutAccumulatedBytes.GetData(), InOutAccumulatedBytes.Num()));
		InOutAccumulatedBytes.Reset();
	}

	return OutMessages.Num();
}

FString FNetFraming::PeekEventType(const uint8* Data, int32 Len, int32 MaxScanBytes)
{
	if (!Data || Len <= 0 || MaxScanBytes <= 0)
	{
		return TEXT("?");
	}
	static const ANSICHAR Key[] = "\"eventType\"";
	static constexpr int32 KeyLen = 11; // strlen("\"eventType\"")
	const int32 ScanEnd = FMath::Min(Len, MaxScanBytes);
	for (int32 i = 0; i + KeyLen < ScanEnd; ++i)
	{
		bool bMatch = true;
		for (int32 k = 0; k < KeyLen; ++k)
		{
			if (Data[i + k] != static_cast<uint8>(Key[k]))
			{
				bMatch = false;
				break;
			}
		}
		if (!bMatch)
		{
			continue;
		}
		// Skip optional whitespace, expect ':', skip whitespace, expect '"'.
		int32 j = i + KeyLen;
		while (j < ScanEnd && (Data[j] == ' ' || Data[j] == '\t' || Data[j] == '\r'))
		{
			++j;
		}
		if (j >= ScanEnd || Data[j] != ':')
		{
			continue;
		}
		++j;
		while (j < ScanEnd && (Data[j] == ' ' || Data[j] == '\t'))
		{
			++j;
		}
		if (j >= ScanEnd || Data[j] != '"')
		{
			continue;
		}
		++j;
		FString Name;
		for (; j < ScanEnd; ++j)
		{
			const uint8 C = Data[j];
			if (C == '"')
			{
				return Name.IsEmpty() ? FString(TEXT("?")) : Name;
			}
			if (C < 32 || C > 126)
			{
				return TEXT("?");
			}
			Name.AppendChar(static_cast<TCHAR>(C));
		}
		return TEXT("?");
	}
	return TEXT("?");
}

FString FNetFraming::PeekHead(const uint8* Data, int32 Len, int32 MaxChars)
{
	FString Out;
	if (!Data || Len <= 0 || MaxChars <= 0)
	{
		return Out;
	}
	const int32 Count = FMath::Min(Len, MaxChars);
	Out.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		const uint8 C = Data[i];
		Out.AppendChar((C >= 32 && C <= 126) ? static_cast<TCHAR>(C) : TEXT('?'));
	}
	return Out;
}
