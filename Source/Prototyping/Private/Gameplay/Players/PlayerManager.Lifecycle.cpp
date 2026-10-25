// Fill out your copyright notice in the Description page of Project Settings.



#include "Gameplay/Players/PlayerManager.h"
#include "Services/TimeSyncService.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "MyGameInstance.h"
#include "Prototyping.h"
#include "CrashDiagnostics.h"


UPlayerManager::UPlayerManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CombatNetworkHandler is now handled centrally by GameInstance
}

void UPlayerManager::Initialize(UNetworkManager* NetworkManager, UPingManager* PingManager)
{
	if (!NetworkManager || !PingManager)
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: Cannot initialize with null NetworkManager or PingManager"));
		return;
	}

	networkManager = NetworkManager;
	pingManager = PingManager;

	// Get the game instance safely
	if (worldContext && IsValid(worldContext))
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

		if (gameInstance && IsValid(gameInstance))
		{
			// Set up PingManager with TimeSyncService integration
			if (pingManager)
			{
				pingManager->SetTimeSyncService(gameInstance->GetTimeSyncService());
			}
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("PlayerManager: GameInstance not found or invalid"));
		}
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: WorldContext is null or invalid"));
	}
}

bool UPlayerManager::IsCombatEvent(const FString& EventType) const
{
	// Define combat-related events that should be handled by CombatNetworkHandler
	return EventType == TEXT("combatInitiation") || 
		   EventType == TEXT("combatResult") ||
		   EventType == TEXT("combatAnimation") ||
		   EventType == TEXT("initiateCombatAction") ||
		   EventType == TEXT("mobTargetLost"); // Add mobTargetLost as combat event
}

// subscribe to the network manager's event
void UPlayerManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr && IsValid(networkManager))
	{
		// Subscribe to the network manager's events for game server.
		// Single-parse path (S05): dispatcher fallback delivers the parsed
		// packet; the legacy FString broadcast is no longer needed here.
		networkManager->DispatcherFor(EServerType::GameServer).RemoveFallbackHandler(this);
		networkManager->DispatcherFor(EServerType::GameServer).AddFallbackHandler(this, &UPlayerManager::ProcessGamePacket);

		// Subscribe to the network manager's events for chunk server data.
		// Single-parse path (S05): dispatcher fallback delivers the parsed
		// packet; the legacy FString broadcast is no longer needed here.
		networkManager->DispatcherFor(EServerType::ChunkServer).RemoveFallbackHandler(this);
		networkManager->DispatcherFor(EServerType::ChunkServer).AddFallbackHandler(this, &UPlayerManager::ProcessChunkPacket);
		// Rejoin trigger for mid-session reconnects (idempotent: Remove first).
		networkManager->OnChunkServerSocketConnected.RemoveDynamic(this, &UPlayerManager::HandleChunkServerSocketConnected);
		networkManager->OnChunkServerSocketConnected.AddDynamic(this, &UPlayerManager::HandleChunkServerSocketConnected);
		UE_LOG(LogConnection, Log, TEXT("PlayerManager: Subscribed to NetworkManager delegates"));
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("PlayerManager: NetworkManager is null or invalid"));
	}
}

// Set world context 
void UPlayerManager::SetWorldContext(UWorld* World)
{
	worldContext = World;
}

// Process game server data
