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

int32 FNetFraming::ExtractPackets(TArray<uint8>& InOutAccumulatedBytes, TArray<FString>& OutMessages)
{
	OutMessages.Reset();

	int32 DelimiterIndex = INDEX_NONE;
	while ((DelimiterIndex = InOutAccumulatedBytes.Find((uint8)'\n')) != INDEX_NONE)
	{
		if (DelimiterIndex > MaxMessageBytes)
		{
			UE_LOG(LogNetPacket, Error, TEXT("FNetFraming: dropping oversized %d-byte packet (cap %d)"),
				DelimiterIndex, MaxMessageBytes);
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
		UE_LOG(LogNetPacket, Error, TEXT("FNetFraming: dropping %d-byte partial without delimiter (cap %d)"),
			InOutAccumulatedBytes.Num(), MaxMessageBytes);
		InOutAccumulatedBytes.Reset();
	}

	return OutMessages.Num();
}
