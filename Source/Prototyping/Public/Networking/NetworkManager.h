// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "Networking/NetworkReceiverWorker.h"
#include "Networking/NetworkSenderWorker.h"
#include "Networking/NetworkDispatcher.h"
#include "Containers/Ticker.h"
#include <Gameplay/UI/MessageBoxPopup.h>
#include "NetworkManager.generated.h"

// Declared in Services/TimeSyncService.h (kept forward here to avoid
// pulling the generated time-sync header into every includer).
enum class EServerType : uint8;

// Forward declarations
class UTimeSyncService;

// Define a delegate or event signature
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoginServerDataReceived, const FString&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameServerDataReceived, const FString&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChunkServerDataReceived, const FString&, Data);


// Delegate to handle that Game Server socket connected
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameServerSocketConnected);

// Delegate to handle that Login Server socket connected
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoginServerSocketConnected);

// Delegate to handle that Chunk Server socket connected
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChunkServerSocketConnected);

/**
 * 
 */
UCLASS()
class PROTOTYPING_API UNetworkManager : public UObject
{
	GENERATED_BODY()
public:
	virtual ~UNetworkManager();

private:
	UPROPERTY()
	UWorld* WorldContext = nullptr;

	// Unique ID for this manager instance — used to make thread names unique
	// across multiple PIE instances so FRunnableThread::Kill targets the right thread.
	int32 InstanceId = 0;

	// One link per EServerType (Login=0, Game=1, Chunk=2). Owns the socket,
	// the receiver/sender workers + threads, and the handshake state.
	// Replaces the old tripled Login/Game/Chunk field sets (S04): a single
	// ConnectLink/PollLink/TeardownLink implementation serves all three.
	struct FServerLink
	{
		FString IP;
		int32 Port = 0;
		// "Login" / "Game" / "Chunk" — log lines, thread names, popup routing.
		FString Tag;
		FSocket* Socket = nullptr;
		NetworkReceiverWorker* Receiver = nullptr;
		FRunnableThread* ReceiverThread = nullptr;
		NetworkSenderWorker* Sender = nullptr;
		FRunnableThread* SenderThread = nullptr;
		// Handshake in flight: the core ticker runs CheckHandshake every 1.0s
		// until workers are up (connect-time) or the budget is spent (popup).
		// Replaces the old per-socket world-TimerManager connect timers, which
		// died on level transitions and left handshakes stuck forever.
		bool ConnectPending = false;
		int32 RetryCount = 0;
		int32 MaxRetries = 3;
		double LastHandshakeCheckSec = 0.0;
	};

	FServerLink Links[3];
	static int32 LinkIndex(EServerType Type) { return static_cast<int32>(Type); }
	FServerLink& LinkFor(EServerType Type) { return Links[LinkIndex(Type)]; }
	const FServerLink& LinkFor(EServerType Type) const { return Links[LinkIndex(Type)]; }

	// Per-connection packet routers (S05), parallel to Links[].
	FNetworkDispatcher LoginDispatcher;
	FNetworkDispatcher GameDispatcher;
	FNetworkDispatcher ChunkDispatcher;

	// Core-ticker handle for poll+handshake (world-independent). Replaces the
	// old world-TimerManager poll/connect timers: the ticker survives level
	// transitions, so handshakes resume automatically and no RestartPolling
	// re-registration is needed. Drained packets still require WorldContext
	// (same as before — queues buffer while travelling).
	FTSTicker::FDelegateHandle PollTickerHandle;

	// Unified internals: one implementation for all three links.
	void InitLinks();
	void EnsureTicker();
	bool TickLinks(float DeltaTime);
	void ConnectLink(EServerType Type);
	void CheckHandshake(EServerType Type);
	void PollLink(EServerType Type);
	void TeardownLink(EServerType Type);
	void SendViaLink(EServerType Type, const FString& Data);
	bool IsLinkConnected(EServerType Type) const;

	// Guard against double Shutdown() calls (GameInstance::Shutdown + destructor)
	bool bIsShutDown = false;

	UPROPERTY()
	TSoftClassPtr<UMessageBoxPopup> MessageBoxPopupClass;

	UPROPERTY()
	UMessageBoxPopup* MsgBoxLoginServer;
	UPROPERTY()
	UMessageBoxPopup* MsgBoxGameServer;
	UPROPERTY()
	UMessageBoxPopup* MsgBoxChunkServer;


public:
	UNetworkManager(const FObjectInitializer& ObjectInitializer);
	void ConnectGameServer();
	void ShowLoginServerConnectionIssuePopup();
	void ShowGameServerConnectionIssuePopup();
	void ShowChunkServerConnectionIssuePopup();
	void SetWorldContext(UWorld* World);
	void SetMessageBoxPopupClass(TSubclassOf<UMessageBoxPopup> InMessageBoxPopupClass);
	void StartPollingLoginServer();
	void StartPollingGameServer();
	void StartPollingChunkServer();
	// Re-register all three poll timers in the current WorldContext.
	// Must be called after SetWorldContext() following a level transition.
	void RestartPolling();
	void ConnectLoginServer();
	void ConnectChunkServer();
	// Mid-session chunk drop: safe chunk-only teardown + ConnectChunkServer()
	// (same 1s / 3-retry budget, popup on exceed). Called on the game thread
	// from PollChunkServerNetworkData when the link is down but a session
	// was established. Keeps the poll timer running by design.
	void HandleChunkMidSessionDrop();
	// Chunk-only twin of Disconnect(): stops/closes/destroys just the chunk
	// socket + workers in the same safe order (Stop -> Detach -> Close ->
	// WaitForCompletion -> Destroy -> delete). Never touches login/game.
	void TeardownChunkLink();
	// DEV-ONLY test hook (called only from devmode.dropsocket): closes the
	// chunk socket without tearing anything down, so the drop detector and
	// the bounded reconnect run exactly like on a real network drop.
	// Safe no-op when there is no live socket. Zero prod impact.
	void DropChunkSocketForTest();
	void SendDataToLoginServer(const FString& Data);
	void SendDataToGameServer(const FString& Data);
	void SendDataToChunkServer(const FString& Data);
	/** Returns true only after the TCP handshake with the login server is done and the sender worker is running. */
	bool IsLoginServerConnected() const;
	/** Returns true only after the TCP handshake with the game server is done and the sender worker is running. */
	bool IsGameServerConnected() const;
	/** Returns true only after the TCP handshake with the chunk server is done and the sender worker is running. */
	bool IsChunkServerConnected() const;
	void PollLoginServerNetworkData();
	void PollGameServerNetworkData();
	void PollChunkServerNetworkData();
	void Shutdown();
	void Disconnect();

    // Process incoming data and extract time sync information
    void ProcessIncomingData(const FString& ReceivedData);

    // Parsed-packet twin (S05): same time-sync extraction on an already-parsed
    // packet, no re-parse. Used by PollLink.
    void ProcessIncomingPacket(const FParsedPacket& Packet);

    // Per-connection eventType routers (S05). Handlers register packet
    // callbacks here instead of re-parsing the FString broadcast to filter.
    // Unmigrated events keep the FString broadcast (incremental migration).
    FNetworkDispatcher& DispatcherFor(EServerType Type);
    const FNetworkDispatcher& DispatcherFor(EServerType Type) const;

    // Get TimeSyncService instance
    UTimeSyncService* GetTimeSyncService();

	UFUNCTION()
	void OnLoginServerConnectionRetry();
	UFUNCTION()
	void OnGameServerConnectionRetry();
	UFUNCTION()
	void OnConnectCancel();
	UFUNCTION()
	void OnChunkServerConnectionRetry();

	// Delegate instance
	FOnLoginServerDataReceived OnLoginServerDataReceived;
	FOnGameServerDataReceived OnGameServerDataReceived;
	FOnChunkServerDataReceived OnChunkServerDataReceived;

	FOnGameServerSocketConnected OnGameServerSocketConnected;
	FOnLoginServerSocketConnected OnLoginServerSocketConnected;
	FOnChunkServerSocketConnected OnChunkServerSocketConnected;
};
