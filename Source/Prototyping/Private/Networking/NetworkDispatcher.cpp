#include "Networking/NetworkDispatcher.h"
#include "Prototyping.h"

TArray<FPacketHandlerEntry>& FNetworkDispatcher::FindOrAdd(const FString& EventType)
{
	TArray<FPacketHandlerEntry>* Found = Handlers.Find(EventType);
	if (!Found)
	{
		Found = &Handlers.Add(EventType);
	}
	return *Found;
}

int32 FNetworkDispatcher::Dispatch(const FParsedPacket& Packet) const
{
	if (!Packet.bValid)
	{
		return 0;
	}
	int32 Fired = 0;
	if (const TArray<FPacketHandlerEntry>* Found = Handlers.Find(Packet.EventType))
	{
		for (const FPacketHandlerEntry& Entry : *Found)
		{
			if (Entry.IsLive())
			{
				Entry.Fn(Packet);
				++Fired;
			}
		}
	}
	for (const FPacketHandlerEntry& Entry : Fallbacks)
	{
		if (Entry.IsLive())
		{
			Entry.Fn(Packet);
			++Fired;
		}
	}
	return Fired;
}
