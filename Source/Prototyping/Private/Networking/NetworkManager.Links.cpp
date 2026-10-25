// Unified Login/Game/Chunk link management (S04).
//
// One FServerLink per EServerType, one implementation for connect / poll /
// teardown. Replaces the old tripled Login/Game/Chunk field sets and the
// world-TimerManager poll/connect timers (which died on level transitions).
// Polling + handshake now run on the core ticker (world-independent); packet
// draining still requires WorldContext, so travel semantics are unchanged.

#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/RetryPolicy.h"
#include "Utils/NetCore/ChunkLinkRule.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"

void UNetworkManager::InitLinks()
{
	Links[LinkIndex(EServerType::LoginServer)].Tag = TEXT("Login");
	Links[LinkIndex(EServerType::GameServer)].Tag = TEXT("Game");
	Links[LinkIndex(EServerType::ChunkServer)].Tag = TEXT("Chunk");
	for (int32 i = 0; i < 3; ++i)
	{
		Links[i].MaxRetries = 3;
	}
}

void UNetworkManager::EnsureTicker()
{
	if (PollTickerHandle.IsValid() || bIsShutDown)
	{
		return;
	}
	PollTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UNetworkManager::TickLinks), 0.0f);
}

bool UNetworkManager::TickLinks(float /*DeltaTime*/)
{
	// Same gate the old world-timer lambdas had: nothing is processed
	// without a world. Queues buffer in the workers while travelling.
	if (bIsShutDown || !WorldContext)
	{
		return true;
	}
	PollLink(EServerType::LoginServer);
	PollLink(EServerType::GameServer);
	PollLink(EServerType::ChunkServer);

	const double NowSec = FPlatformTime::Seconds();
	for (int32 i = 0; i < 3; ++i)
	{
		FServerLink& L = Links[i];
		if (L.ConnectPending && NowSec - L.LastHandshakeCheckSec >= FRetryPolicy::ConnectionRetryIntervalSec)
		{
			L.LastHandshakeCheckSec = NowSec;
			CheckHandshake(static_cast<EServerType>(i));
		}
	}
	return true;
}

void UNetworkManager::ConnectLink(EServerType Type)
{
	FServerLink& L = LinkFor(Type);
	if (!WorldContext)
	{
		UE_LOG(LogConnection, Error, TEXT("ConnectLink %s: no WorldContext"), *L.Tag);
		return;
	}
	if (!L.Socket)
	{
		L.Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(
			NAME_Stream, *FString::Printf(TEXT("%sServerSocket"), *L.Tag), false);
		L.Socket->SetNonBlocking(true);
		L.Socket->SetReuseAddr(true);
		L.Socket->SetRecvErr(true);
		L.Socket->SetNoDelay(true);
	}

	FIPv4Address Addr;
	FIPv4Address::Parse(L.IP, Addr);
	L.Socket->Connect(*FIPv4Endpoint(Addr, L.Port).ToInternetAddr());
	L.RetryCount = 0;
	L.ConnectPending = true;
	L.LastHandshakeCheckSec = 0.0; // handshake runs on the next tick
	EnsureTicker();
}

void UNetworkManager::CheckHandshake(EServerType Type)
{
	if (bIsShutDown || !WorldContext)
	{
		return;
	}
	FServerLink& L = LinkFor(Type);
	if (!L.ConnectPending)
	{
		return;
	}

	if (L.Socket && L.Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected)
	{
		if (L.Receiver)
		{
			return;
		}

		UE_LOG(LogConnection, Log, TEXT("%s Server socket connected."), *L.Tag);
		L.ConnectPending = false;

		L.Receiver = new NetworkReceiverWorker(L.Socket);
		L.Receiver->SetSocketTag(*L.Tag);
		if (UTimeSyncService* TimeSyncSvc = GetTimeSyncService())
		{
			L.Receiver->SetTimeSyncService(TimeSyncSvc);
		}
		L.ReceiverThread = FRunnableThread::Create(L.Receiver,
			*FString::Printf(TEXT("Net%sRecv_%d"), *L.Tag, InstanceId));

		L.Sender = new NetworkSenderWorker(L.Socket);
		if (UTimeSyncService* TimeSyncSvc = GetTimeSyncService())
		{
			L.Sender->SetTimeSyncService(TimeSyncSvc);
		}
		L.SenderThread = FRunnableThread::Create(L.Sender,
			*FString::Printf(TEXT("Net%sSend_%d"), *L.Tag, InstanceId));

		switch (Type)
		{
		case EServerType::LoginServer:
			OnLoginServerSocketConnected.Broadcast();
			break;
		case EServerType::GameServer:
			OnGameServerSocketConnected.Broadcast();
			break;
		case EServerType::ChunkServer:
			OnChunkServerSocketConnected.Broadcast();
			break;
		}
	}
	else
	{
		L.RetryCount++;
		UE_LOG(LogConnection, Verbose, TEXT("Waiting for %s Server socket connection... Retry %d"), *L.Tag, L.RetryCount);
		if (FRetryPolicy::HasExceededMaxRetries(L.RetryCount, L.MaxRetries))
		{
			UE_LOG(LogConnection, Error, TEXT("Failed to connect to %s Server after %d retries."), *L.Tag, L.RetryCount);
			L.ConnectPending = false;
			switch (Type)
			{
			case EServerType::LoginServer:
				if (WorldContext) { ShowLoginServerConnectionIssuePopup(); }
				break;
			case EServerType::GameServer:
				if (WorldContext) { ShowGameServerConnectionIssuePopup(); }
				break;
			case EServerType::ChunkServer:
				if (WorldContext) { ShowChunkServerConnectionIssuePopup(); }
				break;
			}
		}
	}
}

void UNetworkManager::PollLink(EServerType Type)
{
	CRASH_GUARD("NetworkManager::PollLink");
	FServerLink& L = LinkFor(Type);

	FString ReceivedData;
	while (L.Receiver && L.Receiver->GetData(ReceivedData))
	{
		// Single parse per packet (S05): the parsed form feeds time-sync,
		// the per-eventType dispatcher, and the legacy FString broadcast.
		const FParsedPacket Packet = JSONParser::ParsePacket(ReceivedData);

		UE_LOG(LogNetPacket, Verbose, TEXT("Received %s Server data: %s"), *L.Tag, *ReceivedData);

		// Process time sync data (no re-parse — works on the parsed packet)
		ProcessIncomingPacket(Packet);

		// Route to packet handlers registered for this eventType.
		DispatcherFor(Type).Dispatch(Packet);

		// Legacy compat broadcast (Blueprints + not-yet-migrated handlers).
		// Migrated handlers unsubscribe from this once they register above.
		switch (Type)
		{
		case EServerType::LoginServer:
			OnLoginServerDataReceived.Broadcast(ReceivedData);
			break;
		case EServerType::GameServer:
			OnGameServerDataReceived.Broadcast(ReceivedData);
			break;
		case EServerType::ChunkServer:
			OnChunkServerDataReceived.Broadcast(ReceivedData);
			break;
		}
	}

	if (Type != EServerType::ChunkServer)
	{
		return;
	}

	// Mid-session drop watch (game thread, ~1 tick). Connect-time failures
	// are owned by the handshake budget above; this only fires once a
	// session exists (workers up) and the link goes down. The pure predicate
	// owns the decision; the handler reuses the connect-time budget + popup.
	if (bIsShutDown || !WorldContext)
	{
		return;
	}
	if (!L.Receiver || !L.Sender)
	{
		return;
	}
	const bool bWorkerDrop = L.Receiver->ConsumeDropObserved();
	const bool bSocketDown = L.Socket
		&& L.Socket->GetConnectionState() != ESocketConnectionState::SCS_Connected;
	if (FChunkLinkRule::ShouldStartReconnect(true, bWorkerDrop || bSocketDown, L.ConnectPending))
	{
		HandleChunkMidSessionDrop();
	}
}

void UNetworkManager::TeardownLink(EServerType Type)
{
	FServerLink& L = LinkFor(Type);

	// Same safe order as the old Disconnect(): Stop -> Detach -> Close ->
	// WaitForCompletion -> Destroy -> delete. Never destroy a socket while
	// a worker thread may still call a virtual method on it.
	if (L.Sender) { L.Sender->Stop(); }
	if (L.Receiver) { L.Receiver->Stop(); }

	if (L.Receiver) { L.Receiver->DetachSocket(); }
	if (L.Sender) { L.Sender->DetachSocket(); }

	if (L.Socket != nullptr)
	{
		L.Socket->Close();
	}

	auto ShutdownThread = [](FRunnable* Worker, FRunnableThread*& Thread)
	{
		if (Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}
	};
	ShutdownThread(L.Receiver, L.ReceiverThread);
	ShutdownThread(L.Sender, L.SenderThread);

	if (L.Socket != nullptr)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(L.Socket);
		L.Socket = nullptr;
	}

	// Reassembly observability (S13-C3): per-link wireFrag totals. The worker
	// thread joined above, so its counters are safe to read. Quiet when
	// nothing fragmented (Log, not Warning — teardown is routine).
	if (L.Receiver)
	{
		const FWireFragStats& Stats = L.Receiver->GetFragStats();
		if (Stats.StreamsCompleted > 0 || Stats.FramesMalformed > 0)
		{
			UE_LOG(LogConnection, Log, TEXT("Link %s teardown: wireFrag streams=%llu frames=%llu malformed=%llu bytes=%llu"),
				*L.Receiver->GetSocketTag(), Stats.StreamsCompleted, Stats.FramesSeen,
				Stats.FramesMalformed, Stats.BytesReassembled);
		}
	}

	delete L.Receiver; L.Receiver = nullptr;
	delete L.Sender; L.Sender = nullptr;
	L.ConnectPending = false;
}

FNetworkDispatcher& UNetworkManager::DispatcherFor(EServerType Type)
{
	switch (Type)
	{
	case EServerType::LoginServer:
		return LoginDispatcher;
	case EServerType::GameServer:
		return GameDispatcher;
	case EServerType::ChunkServer:
	default:
		return ChunkDispatcher;
	}
}

const FNetworkDispatcher& UNetworkManager::DispatcherFor(EServerType Type) const
{
	switch (Type)
	{
	case EServerType::LoginServer:
		return LoginDispatcher;
	case EServerType::GameServer:
		return GameDispatcher;
	case EServerType::ChunkServer:
	default:
		return ChunkDispatcher;
	}
}

void UNetworkManager::SendViaLink(EServerType Type, const FString& Data)
{
	FServerLink& L = LinkFor(Type);
	if (L.Sender != nullptr)
	{
		L.Sender->EnqueueDataForSending(Data);
		return;
	}
	if (Type == EServerType::ChunkServer && L.ConnectPending)
	{
		// Reconnect in flight (mid-session drop or connect-time retries):
		// packets have nowhere to go — drop quietly. Fresh positions arrive
		// on their own after rejoin, and Error here would spam every tick.
		UE_LOG(LogConnection, Verbose, TEXT("SendDataToChunkServer dropped while reconnecting"));
		return;
	}
	UE_LOG(LogConnection, Error, TEXT("Sender %s Server Worker is null"), *L.Tag);
}

bool UNetworkManager::IsLinkConnected(EServerType Type) const
{
	return LinkFor(Type).Sender != nullptr;
}
