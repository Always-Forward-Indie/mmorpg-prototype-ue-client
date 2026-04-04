// Fill out your copyright notice in the Description page of Project Settings.

#include "Networking/PingManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UPingManager::UPingManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	networkManager = nullptr;
	worldContext = nullptr;
	MonitorStatsWidget = nullptr;
	TimeSyncService = nullptr;
}

void UPingManager::Initialize(UNetworkManager* NetworkManager, UMonitorStatsWidget* MonitorStats)
{
	// Initialize the network manager
	networkManager = NetworkManager;

	// Initialize the monitor stats widget
	MonitorStatsWidget = MonitorStats;
	
	// Get TimeSyncService from NetworkManager if available
	if (networkManager)
	{
		TimeSyncService = networkManager->GetTimeSyncService();
	}
	
	UE_LOG(LogPing, Log, TEXT("PingManager: Initialized with TimeSyncService integration"));
}

void UPingManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

void UPingManager::SetTimeSyncService(UTimeSyncService* InTimeSyncService)
{
	TimeSyncService = InTimeSyncService;
	UE_LOG(LogPing, Log, TEXT("PingManager: TimeSyncService reference set"));
}

void UPingManager::StartPingUpdates()
{
	if (!worldContext)
	{
		UE_LOG(LogPing, Error, TEXT("PingManager: Cannot start ping updates - no world context"));
		return;
	}
	
	if (!TimeSyncService)
	{
		UE_LOG(LogPing, Error, TEXT("PingManager: Cannot start ping updates - no TimeSyncService"));
		return;
	}
	
	// Update ping display every 3 seconds
	const float UpdateInterval = 3.0f;
	worldContext->GetTimerManager().SetTimer(PingUpdateTimerHandle, this, &UPingManager::OnPingUpdateTimer, UpdateInterval, true);
	
	// Start sending ping requests every 1 seconds
	const float PingRequestInterval = 1.0f;
	worldContext->GetTimerManager().SetTimer(PingRequestTimerHandle, this, &UPingManager::OnPingRequestTimer, PingRequestInterval, true);
	
	// Update immediately
	UpdatePingDisplay();
	
	UE_LOG(LogPing, Log, TEXT("PingManager: Started ping updates with %.1f second interval and ping requests with %.1f second interval"), 
		UpdateInterval, PingRequestInterval);
}

void UPingManager::StopPingUpdates()
{
	if (worldContext)
	{
		worldContext->GetTimerManager().ClearTimer(PingUpdateTimerHandle);
		worldContext->GetTimerManager().ClearTimer(PingRequestTimerHandle);
		UE_LOG(LogPing, Log, TEXT("PingManager: Stopped ping updates and ping requests"));
	}
}

void UPingManager::RestartPingUpdates()
{
	if (!worldContext)
	{
		UE_LOG(LogPing, Error, TEXT("PingManager::RestartPingUpdates - WorldContext is null"));
		return;
	}

	// Handles from the old world are stale - invalidate before re-registering
	PingUpdateTimerHandle.Invalidate();
	PingRequestTimerHandle.Invalidate();

	StartPingUpdates();
	UE_LOG(LogPing, Log, TEXT("PingManager: Ping timers restarted in new world"));
}

float UPingManager::GetServerPing(EServerType ServerType) const
{
	if (!TimeSyncService)
	{
		return 0.0f;
	}
	
	return TimeSyncService->GetNetworkLatencyMs(ServerType);
}

void UPingManager::SendPingRequestToAllServers()
{
	if (!networkManager || !TimeSyncService)
	{
		UE_LOG(LogPing, Log, TEXT("PingManager: Cannot send ping requests - missing NetworkManager or TimeSyncService"));
		return;
	}

	// Send ping request to Login Server
	SendPingRequestToServer(EServerType::LoginServer);
	
	// Send ping request to Game Server
	SendPingRequestToServer(EServerType::GameServer);
	
	// Send ping request to Chunk Server
	SendPingRequestToServer(EServerType::ChunkServer);
}

void UPingManager::SendPingRequestToServer(EServerType ServerType)
{
	if (!networkManager || !TimeSyncService)
	{
		return;
	}

	// Generate unique request ID for this ping
	FString RequestId = TimeSyncService->GenerateAndRegisterSyncRequest(ServerType);
	
	// Create the ping request JSON per protocol spec (1.4 pingClient)
	TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
	HeaderObject->SetStringField("eventType", "pingClient");
	
	// Per protocol: header must include clientId and hash
	if (worldContext)
	{
		UMyGameInstance* MyGI = Cast<UMyGameInstance>(worldContext->GetGameInstance());
		if (MyGI)
		{
			HeaderObject->SetNumberField("clientId", MyGI->GetCurrentClientID());
			HeaderObject->SetStringField("hash", MyGI->GetCurrentClientHash());
		}
	}
	
	// Per protocol: timestamps sub-object with clientSendMsEcho and requestId
	TSharedPtr<FJsonObject> TimestampsObject = MakeShareable(new FJsonObject);
	int64 ClientSendMs = TimeSyncService->GetCurrentClientTimeMs();
	TimestampsObject->SetNumberField("clientSendMsEcho", ClientSendMs);
	TimestampsObject->SetStringField("requestId", RequestId);
	HeaderObject->SetObjectField("timestamps", TimestampsObject);

	// Create the main JSON object
	TSharedPtr<FJsonObject> MainJsonObject = MakeShareable(new FJsonObject);
	MainJsonObject->SetObjectField("header", HeaderObject);
	
	// Add empty body
	TSharedPtr<FJsonObject> BodyObject = MakeShareable(new FJsonObject);
	MainJsonObject->SetObjectField("body", BodyObject);

	// Serialize to string
	FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
	FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);

	// Remove newline characters
	OutputString.ReplaceInline(TEXT("\n"), TEXT(""));
	OutputString.ReplaceInline(TEXT("\r"), TEXT(""));

	// Send to appropriate server
	switch (ServerType)
	{
		case EServerType::LoginServer:
			networkManager->SendDataToLoginServer(OutputString);
			break;
			
		case EServerType::GameServer:
			networkManager->SendDataToGameServer(OutputString);
			break;
			
		case EServerType::ChunkServer:
			networkManager->SendDataToChunkServer(OutputString);
			break;
			
		default:
			UE_LOG(LogPing, Log, TEXT("PingManager: Unknown server type for ping request"));
			break;
	}
}

void UPingManager::UpdatePingDisplay()
{
	if (!IsValid(MonitorStatsWidget) || !IsValid(TimeSyncService))
	{
		return;
	}
	
	// Get ping values from TimeSyncService for each server
	float LoginServerPing = TimeSyncService->GetNetworkLatencyMs(EServerType::LoginServer);
	float GameServerPing = TimeSyncService->GetNetworkLatencyMs(EServerType::GameServer);
	float ChunkServerPing = TimeSyncService->GetNetworkLatencyMs(EServerType::ChunkServer);
	
	// Check if we have valid sync data for each server
	bool bLoginValid = TimeSyncService->IsTimeSyncValid(EServerType::LoginServer);
	bool bGameValid = TimeSyncService->IsTimeSyncValid(EServerType::GameServer);
	bool bChunkValid = TimeSyncService->IsTimeSyncValid(EServerType::ChunkServer);
	
	// Format and display Login Server ping
	if (bLoginValid && LoginServerPing > 0.0f)
	{
		FString LoginPingText = FString::Printf(TEXT("%.1f ms"), LoginServerPing);
		MonitorStatsWidget->SetLoginServerPingValue(LoginPingText);
		UE_LOG(LogPing, VeryVerbose, TEXT("PingManager: Updated Login Server ping: %s"), *LoginPingText);
	}
	else
	{
		MonitorStatsWidget->SetLoginServerPingValue(TEXT("-- ms"));
		UE_LOG(LogPing, VeryVerbose, TEXT("PingManager: Login Server ping not available (valid: %s, ping: %.1f)"), 
			bLoginValid ? TEXT("true") : TEXT("false"), LoginServerPing);
	}
	
	// Format and display Game Server ping
	if (bGameValid && GameServerPing > 0.0f)
	{
		FString GamePingText = FString::Printf(TEXT("%.1f ms"), GameServerPing);
		MonitorStatsWidget->SetGameServerPingValue(GamePingText);
		UE_LOG(LogPing, VeryVerbose, TEXT("PingManager: Updated Game Server ping: %s"), *GamePingText);
	}
	else
	{
		MonitorStatsWidget->SetGameServerPingValue(TEXT("-- ms"));
		UE_LOG(LogPing, VeryVerbose, TEXT("PingManager: Game Server ping not available (valid: %s, ping: %.1f)"), 
			bGameValid ? TEXT("true") : TEXT("false"), GameServerPing);
	}
	
	// Format and display Chunk Server ping
	if (bChunkValid && ChunkServerPing > 0.0f)
	{
		FString ChunkPingText = FString::Printf(TEXT("%.1f ms"), ChunkServerPing);
		MonitorStatsWidget->SetChunkServerPingValue(ChunkPingText);
		UE_LOG(LogPing, VeryVerbose, TEXT("PingManager: Updated Chunk Server ping: %s"), *ChunkPingText);
	}
	else
	{
		MonitorStatsWidget->SetChunkServerPingValue(TEXT("-- ms"));
		UE_LOG(LogPing, VeryVerbose, TEXT("PingManager: Chunk Server ping not available (valid: %s, ping: %.1f)"), 
			bChunkValid ? TEXT("true") : TEXT("false"), ChunkServerPing);
	}
}

void UPingManager::OnPingUpdateTimer()
{
	UpdatePingDisplay();
}

void UPingManager::OnPingRequestTimer()
{
	SendPingRequestToAllServers();
}

// Legacy method - now deprecated but kept for compatibility
void UPingManager::CalculatePingTime(const TArray<FDateTime>& SendTimes, const TArray<FDateTime>& ReceiveTimes, const FString& serverName)
{
	UE_LOG(LogPing, Log, TEXT("PingManager: CalculatePingTime is deprecated. Use TimeSyncService instead for server: %s"), *serverName);
	
	// For backwards compatibility, we still update the display using TimeSyncService
	UpdatePingDisplay();
}
