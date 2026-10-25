// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "CrashDiagnostics.h"

//constructor
UMOBManager::UMOBManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UMOBManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;

	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());
	}

	if (gameInstance)
	{
		UE_LOG(LogGameplay, Warning, TEXT("GameInstance found"));
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("GameInstance not found"));
	}
}

void UMOBManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

// subscribe to the network manager's event
void UMOBManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogGameplay, Warning, TEXT("Network Manager found and subscribed to ChunkServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogGameplay, Warning, TEXT("Network Manager is valid"));

			// Subscribe to the network manager's events for chunk server data
			networkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UMOBManager::ProcessGameServerData);
			networkManager->OnChunkServerDataReceived.AddDynamic(this, &UMOBManager::ProcessGameServerData);
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("Network manager not found"));
	}
}

// Set world context 
void UMOBManager::SetWorldContext(UWorld* World)
{
	const bool bWasNull = (worldContext == nullptr);
	worldContext = World;

	if (bWasNull && worldContext)
	{
		FlushPendingSpawns();
	}
}

// Check if event is combat-related and should be handled by CombatNetworkHandler
bool UMOBManager::IsCombatEvent(const FString& EventType) const
{
	// Define combat-related events that should be handled by CombatNetworkHandler
	return EventType == TEXT("combatInitiation") || 
		   EventType == TEXT("combatResult") ||
		   EventType == TEXT("combatAnimation") ||
		   EventType == TEXT("initiateCombatAction") ||
		   EventType == TEXT("mobTargetLost"); // Add mobTargetLost as combat event
}

void UMOBManager::ClearLockedTargetOnAllPlayers(ABasicMOB* Mob)
{
	if (!gameInstance || !IsValid(Mob)) return;

	for (TPair<int32, ABasicPlayer*>& Pair : gameInstance->SpawnedPlayers)
	{
		ABasicPlayer* Player = Pair.Value;
		if (IsValid(Player) && Player->GetLockedTarget() == Mob)
		{
			Player->ClearLockedTarget();
		}
	}
}

// Process game server data
