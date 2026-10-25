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

	// Link tags / budgets first; IP/Port come from server_config.json below.
	InitLinks();

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
			LinkFor(EServerType::LoginServer).IP = JsonObject->GetObjectField(TEXT("LoginServer"))->GetStringField(TEXT("IP"));
			LinkFor(EServerType::LoginServer).Port = JsonObject->GetObjectField(TEXT("LoginServer"))->GetIntegerField(TEXT("Port"));

			LinkFor(EServerType::GameServer).IP = JsonObject->GetObjectField(TEXT("GameServer"))->GetStringField(TEXT("IP"));
			LinkFor(EServerType::GameServer).Port = JsonObject->GetObjectField(TEXT("GameServer"))->GetIntegerField(TEXT("Port"));

			LinkFor(EServerType::ChunkServer).IP = JsonObject->GetObjectField(TEXT("ChunkServer"))->GetStringField(TEXT("IP"));
			LinkFor(EServerType::ChunkServer).Port = JsonObject->GetObjectField(TEXT("ChunkServer"))->GetIntegerField(TEXT("Port"));

			UE_LOG(LogConnection, Log, TEXT("Login Server: %s:%d"), *LinkFor(EServerType::LoginServer).IP, LinkFor(EServerType::LoginServer).Port);
			UE_LOG(LogConnection, Log, TEXT("Game Server: %s:%d"), *LinkFor(EServerType::GameServer).IP, LinkFor(EServerType::GameServer).Port);
			UE_LOG(LogConnection, Log, TEXT("Chunk Server: %s:%d"), *LinkFor(EServerType::ChunkServer).IP, LinkFor(EServerType::ChunkServer).Port);
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("Failed to parse JSON config file."));
		}
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("Failed to load config file: %s"), *ConfigFilePath);
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
	// World-TimerManager polling is gone (S04): one core ticker serves all
	// links and survives level transitions. Kept for call-site compatibility.
	EnsureTicker();
	UE_LOG(LogConnection, Verbose, TEXT("Polling (core ticker) ensured for login server data."));
}

void UNetworkManager::StartPollingGameServer()
{
	EnsureTicker();
	UE_LOG(LogConnection, Verbose, TEXT("Polling (core ticker) ensured for game server data."));
}

// start polling data from chunk server
void UNetworkManager::StartPollingChunkServer()
{
	EnsureTicker();
	UE_LOG(LogConnection, Verbose, TEXT("Polling (core ticker) ensured for chunk server data."));
}

void UNetworkManager::RestartPolling()
{
	if (!WorldContext)
	{
		UE_LOG(LogConnection, Error, TEXT("NetworkManager::RestartPolling - WorldContext is null, cannot restart polling"));
		return;
	}

	// Nothing to re-register: the core ticker never belonged to the old
	// world. Ensure it is running (no-op when it already is).
	UE_LOG(LogConnection, Log, TEXT("NetworkManager::RestartPolling - core ticker survives travel, ensuring it runs"));
	EnsureTicker();
}

