// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/ItemManager.h"
#include "Prototyping.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationToastWidget.h"
#include "Kismet/GameplayStatics.h"

#include "Engine/AssetManager.h"
#include "CrashDiagnostics.h"

// Sets default values for this component's properties
UItemManager::UItemManager()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.
	PrimaryComponentTick.bCanEverTick = true;

	// Initialize references
	networkManager = nullptr;
	gameInstance = nullptr;
	worldContext = nullptr;
}

void UItemManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;

	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

		if (gameInstance)
		{
			UE_LOG(LogGameplay, Warning, TEXT("GameInstance found in ItemManager"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("GameInstance not found in ItemManager"));
		}
	}
}

void UItemManager::SetWorldContext(UWorld* World)
{
	const bool bWasNull = (worldContext == nullptr);
	worldContext = World;

	if (bWasNull && worldContext)
	{
		FlushPendingEvents();
	}
}

void UItemManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

void UItemManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogGameplay, Warning, TEXT("ItemManager: Network Manager found and subscribed to ChunkServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogGameplay, Warning, TEXT("ItemManager: Network Manager is valid"));

			// Subscribe to the network manager's events.
			// Single-parse path (S05): dispatcher fallback delivers the parsed packet.
			networkManager->DispatcherFor(EServerType::ChunkServer).RemoveFallbackHandler(this);
			networkManager->DispatcherFor(EServerType::ChunkServer).AddFallbackHandler(this, &UItemManager::HandleChunkPacket);
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("ItemManager: Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("ItemManager: Network manager not found"));
	}
}

