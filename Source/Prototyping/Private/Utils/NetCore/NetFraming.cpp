#include "Utils/NetCore/NetFraming.h"
#include "Prototyping.h"

int32 FNetFraming::ExtractFramedMessages(TArray<uint8>& InOutAccumulatedBytes, TArray<FString>& OutMessages,
const TCHAR* SocketTag)
{
	OutMessages.Reset();

	while (InOutAccumulatedBytes.Num() >= 4)
	{
		const uint32 FrameLen = ReadU32LE(InOutAccumulatedBytes.GetData());
		if (FrameLen > static_cast<uint32>(MaxFrameBytes))
		{
			UE_LOG(LogNetPacket, Error, TEXT("FNetFraming: dropping %d-byte frame (cap %d) socket=%s — framing violation, tearing down"),
				FrameLen, MaxFrameBytes, SocketTag ? SocketTag : TEXT("?"));
			InOutAccumulatedBytes.Reset();
			return -1;
		}
		if (InOutAccumulatedBytes.Num() < 4 + static_cast<int32>(FrameLen))
		{
			break; // partial frame: wait for more TCP chunks
		}
		if (FrameLen == 0)
		{
			// Empty frame: protocol violation, but resynchronizable —
			// skip the prefix and continue (a length desync is impossible:
			// lengths are exact, only the sender can misframe).
			UE_LOG(LogNetPacket, Error, TEXT("FNetFraming: zero-length frame socket=%s — skipping prefix"),
				SocketTag ? SocketTag : TEXT("?"));
			InOutAccumulatedBytes.RemoveAt(0, 4);
			continue;
		}
		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(InOutAccumulatedBytes.GetData() + 4), FrameLen);
		FString Message(Converter.Length(), Converter.Get());
		InOutAccumulatedBytes.RemoveAt(0, 4 + FrameLen);

		Message.TrimStartAndEndInline();
		if (!Message.IsEmpty())
		{
			OutMessages.Add(MoveTemp(Message));
		}
	}

	return OutMessages.Num();
}
