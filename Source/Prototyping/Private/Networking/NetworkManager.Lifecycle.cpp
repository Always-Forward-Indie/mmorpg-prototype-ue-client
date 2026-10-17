// Fill out your copyright notice in the Description page of Project Settings.


#include "Networking/NetworkManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonUtilities.h"
#include <Kismet/KismetSystemLibrary.h>
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/RetryPolicy.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"


// File-scope counter � atomically incremented for each new UNetworkManager instance.
// Kept here (not in the header) so UHT never sees it.
static int32 GNetworkManagerNextInstanceId = 0;

UNetworkManager::UNetworkManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceId = FPlatformAtomics::InterlockedIncrement(&GNetworkManagerNextInstanceId);
	UE_LOG(LogConnection, Log, TEXT("Network Manager Constructor called"));

	FString ConfigFilePath;

	// Detect whether we are running in the Unreal Editor or a packaged game
	if (FPaths::FileExists(FPaths::ProjectDir() + TEXT("server_config.json")))
	{
		// If running in Unreal Editor, use the project's root folder
		ConfigFilePath = FPaths::ProjectDir() + TEXT("server_config.json");
	}
	else
	{
		// If running a packaged game, use the same directory as the executable
		ConfigFilePath = FPaths::LaunchDir() + TEXT("server_config.json");
	}

	UE_LOG(LogConnection, Log, TEXT("Config file path: %s"), *ConfigFilePath);

	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *ConfigFilePath))
	{
		UE_LOG(LogConnection, Log, TEXT("Successfully loaded config file from: %s"), *ConfigFilePath);

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			LoginServerIP = JsonObject->GetObjectField(TEXT("LoginServer"))->GetStringField(TEXT("IP"));
			LoginServerPort = JsonObject->GetObjectField(TEXT("LoginServer"))->GetIntegerField(TEXT("Port"));

			GameServerIP = JsonObject->GetObjectField(TEXT("GameServer"))->GetStringField(TEXT("IP"));
			GameServerPort = JsonObject->GetObjectField(TEXT("GameServer"))->GetIntegerField(TEXT("Port"));

			ChunkServerIP = JsonObject->GetObjectField(TEXT("ChunkServer"))->GetStringField(TEXT("IP"));
			ChunkServerPort = JsonObject->GetObjectField(TEXT("ChunkServer"))->GetIntegerField(TEXT("Port"));

			UE_LOG(LogConnection, Log, TEXT("Login Server: %s:%d"), *LoginServerIP, LoginServerPort);
			UE_LOG(LogConnection, Log, TEXT("Game Server: %s:%d"), *GameServerIP, GameServerPort);
			UE_LOG(LogConnection, Log, TEXT("Chunk Server: %s:%d"), *ChunkServerIP, ChunkServerPort);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON config file."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load config file: %s"), *ConfigFilePath);
	}
}


void UNetworkManager::SetWorldContext(UWorld* World) {
	WorldContext = World; // Set the world context
}

void UNetworkManager::SetMessageBoxPopupClass(TSubclassOf<UMessageBoxPopup> InMessageBoxPopupClass)
{
	MessageBoxPopupClass = TSoftClassPtr<UMessageBoxPopup>(InMessageBoxPopupClass);
}

void UNetworkManager::StartPollingLoginServer()
{
	const float PollIntervalLoginServerData = 0.0001f; // Adjust as necessary
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkLoginServerPollTimerHandle);
		WorldContext->GetTimerManager().SetTimer(NetworkLoginServerPollTimerHandle, this, &UNetworkManager::PollLoginServerNetworkData, PollIntervalLoginServerData, true);
		UE_LOG(LogConnection, Verbose, TEXT("Polling timer for login server data set up successfully."));
	}
}

void UNetworkManager::StartPollingGameServer()
{
	const float PollIntervalGameServerData = 0.001f; // Adjust as necessary
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkGameServerPollTimerHandle);
		WorldContext->GetTimerManager().SetTimer(NetworkGameServerPollTimerHandle, this, &UNetworkManager::PollGameServerNetworkData, PollIntervalGameServerData, true);
		UE_LOG(LogConnection, Verbose, TEXT("Polling timer for game server data set up successfully."));
	}
}

// start polling data from chunk server
void UNetworkManager::StartPollingChunkServer()
{
	const float PollIntervalChunkServerData = 0.001f; // Adjust as necessary
	if (WorldContext)
	{
		WorldContext->GetTimerManager().ClearTimer(NetworkChunkServerPollTimerHandle);
		WorldContext->GetTimerManager().SetTimer(NetworkChunkServerPollTimerHandle, this, &UNetworkManager::PollChunkServerNetworkData, PollIntervalChunkServerData, true);
		UE_LOG(LogConnection, Verbose, TEXT("Polling timer for chunk server data set up successfully."));
	}
}

void UNetworkManager::RestartPolling()
{
	if (!WorldContext)
	{
		UE_LOG(LogConnection, Error, TEXT("NetworkManager::RestartPolling - WorldContext is null, cannot restart polling timers"));
		return;
	}

	UE_LOG(LogConnection, Log, TEXT("NetworkManager::RestartPolling - Re-registering poll timers in new world"));

	// The old TimerManager is gone after level transition - register fresh timers
	// in the new world's TimerManager. ClearTimer on a stale handle is a no-op so
	// it is safe to call even if the handle references the destroyed old world.
	NetworkLoginServerPollTimerHandle.Invalidate();
	NetworkGameServerPollTimerHandle.Invalidate();
	NetworkChunkServerPollTimerHandle.Invalidate();

	StartPollingLoginServer();
	StartPollingGameServer();
	StartPollingChunkServer();
}

