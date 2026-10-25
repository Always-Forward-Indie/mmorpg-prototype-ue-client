#include "Gameplay/Items/HarvestManager.h"
#include "Prototyping.h"
#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "TimerManager.h"
#include "CrashDiagnostics.h"


UHarvestManager::UHarvestManager()
{
	// Initialize references
	networkManager = nullptr;
	gameInstance = nullptr;
	worldContext = nullptr;
	HarvestProgressWidget = nullptr;
	HarvestLootWidget = nullptr;

	// Initialize harvest state
	bIsHarvesting = false;
	CurrentCorpseUID = 0;
	HarvestStartTime = 0.0f;
	HarvestDuration = 0.0f;
	ServerStartTime = 0;

	// Initialize settings
	MaxHarvestDistance = 180.0f;
	bAutoShowLootWindow = true;

	// Initialize loot data
	CurrentAvailableLoot.Empty();
	
	UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Constructor - UObject version"));
}

void UHarvestManager::BeginPlay()
{
	// Start ticking when harvest manager begins play
	StartTicking();
}

void UHarvestManager::Tick(float DeltaTime)
{
	CRASH_GUARD("HarvestManager::Tick");

	// Update harvest progress if harvesting
	if (bIsHarvesting)
	{
		// Add debug logging to see if tick is working
		static float LastTickLog = 0.0f;
		UWorld* World = GetValidWorld();
		if (World)
		{
			float CurrentTime = World->GetTimeSeconds();
			if (CurrentTime - LastTickLog > 1.0f) // Log every second
			{
				UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Tick - bIsHarvesting: %s, CurrentTime: %.2f, HarvestStartTime: %.2f, HarvestDuration: %.0f"), 
					bIsHarvesting ? TEXT("true") : TEXT("false"), CurrentTime, HarvestStartTime, HarvestDuration);
				LastTickLog = CurrentTime;
			}
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Tick - GetValidWorld() returned null!"));
		}
		
		UpdateHarvestProgress(DeltaTime);
	}
}

void UHarvestManager::Initialize(UNetworkManager* NetworkManager)
{
	networkManager = NetworkManager;

	// Get the game instance
	if (worldContext)
	{
		gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

		if (gameInstance)
		{
			UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: GameInstance found"));
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("HarvestManager: GameInstance not found"));
		}
	}

	// Start ticking after initialization
	BeginPlay();
}

void UHarvestManager::SetWorldContext(UWorld* World)
{
	// Always stop ticking and invalidate the handle before changing world,
	// regardless of whether the new world is valid. This prevents the timer
	// from firing on a destroyed TimerManager after a level transition.
	StopTicking();
	TickTimerHandle.Invalidate();

	worldContext = World;
	UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: World context set to %s"), World ? TEXT("Valid") : TEXT("NULL"));

	// Re-register the tick timer in the new world after a level transition
	if (worldContext)
	{
		StartTicking();
	}
}

void UHarvestManager::SetGameInstance(UMyGameInstance* GameInstance)
{
	gameInstance = GameInstance;
}

void UHarvestManager::SubscribeToNetworkManager()
{
	if (networkManager != nullptr)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Network Manager found and subscribed to events"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Network Manager is valid"));

			// Subscribe to the network manager's events.
			// Single-parse path (S05): dispatcher fallback delivers the parsed packet.
			networkManager->DispatcherFor(EServerType::ChunkServer).RemoveFallbackHandler(this);
			networkManager->DispatcherFor(EServerType::ChunkServer).AddFallbackHandler(this, &UHarvestManager::HandleChunkPacket);
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Network Manager is not valid"));
		}
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Network manager not found"));
	}
}

